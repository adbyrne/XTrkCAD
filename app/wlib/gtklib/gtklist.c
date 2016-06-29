/** \file gtklist.c
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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "gtkint.h"
#include "i18n.h"

#define ROW_HEIGHT (15)
#define PIX_TEXT_SEP (5)
static char ListItemDataKey[] = "ListItemDataKey";

/*
 *****************************************************************************
 *
 * List Boxes
 *
 *****************************************************************************
 */

typedef struct wListItem_t * wListItem_p;

struct wList_t {
		WOBJ_COMMON
		GtkWidget *list;
		int count;
		int number;
		int colCnt;
		wPos_t *colWidths;
		wBool_t *colRightJust;
		int last;
		wPos_t listX;
		long * valueP;
		wListCallBack_p action;
		int recursion;
		int editted;
		int editable;
		};


struct wListItem_t {
		wBool_t active;
		void * itemData;
		const char * label;
		GtkLabel * labelG;
		wBool_t selected;
		wList_p listP;
		};


static wListItem_p getListItem(
		wList_p b,
		wIndex_t inx,
		GList ** childR )
{
	GList * child;
	GtkObject * listItem;
	wListItem_p id_p;
	if (childR)
		*childR = NULL;
	if (b->list == 0) abort();
	if (inx < 0)
		return NULL;
	if ( b->type == B_LIST )
		return (wListItem_p)gtk_clist_get_row_data( GTK_CLIST(b->list), inx );

	for ( child=GTK_LIST(b->list)->children; inx>0&&child; child=child->next,inx-- );
	if (child==NULL) {
		fprintf( stderr, "wListGetValues - End Of List\n" );
		return NULL;
	}
	listItem = GTK_OBJECT(child->data);
	id_p = (wListItem_p)gtk_object_get_data(listItem, ListItemDataKey );
	if (id_p==NULL) {
		fprintf( stderr, "wListGetValues - id_p == NULL\n" );
		return NULL;
	}
	if (childR)
		*childR = child;
	return id_p;
}


EXPORT void wListClear(
		wList_p b )		/* List */
/*
Remove all entries from the list <b>.
*/
{
	if (b->list == 0) abort();
	b->recursion++;
	if ( b->type == B_DROPLIST )
		gtk_list_clear_items( GTK_LIST(b->list), 0, b->count );
	else
		gtk_clist_clear( GTK_CLIST(b->list) );
	b->recursion--;
	b->last = -1;
	b->count = 0;
}


EXPORT void wListSetIndex(
		wList_p b,		/* List */
		int val )		/* Index */
/*
Makes the <val>th entry (0-origin) the current selection.
If <val> if '-1' then no entry is selected.
*/
{
	int cur;
	wListItem_p id_p;

	if (b->widget == 0) abort();
	b->recursion++;
	cur = b->last;
	if ( b->type == B_DROPLIST ) {
	if ((b->option&BL_NONE)!=0 && val < 0) {
		if (cur != -1) {
			gtk_list_unselect_item( GTK_LIST(b->list), cur );
		}
	} else {
		if (cur != -1)
			gtk_list_unselect_item( GTK_LIST(b->list), cur );
		if (val != -1)
			gtk_list_select_item( GTK_LIST(b->list), val );
	}
	} else {
		if (cur != -1) {
			gtk_clist_unselect_row( GTK_CLIST(b->list), cur, -1 );
			id_p = getListItem( b, cur, NULL );
			if ( id_p )
				id_p->selected = FALSE;
		}
		if (val != -1) {
			gtk_clist_select_row( GTK_CLIST(b->list), val, -1 );
			id_p = getListItem( b, val, NULL );
			if ( id_p )
				id_p->selected = TRUE;
		}
	}
	b->last = val;
	b->recursion--;
}


static void parseLabelStr(
		const char * labelStr,
		int count,
		char * * * texts )
{
	static char * labelBuffer;
	static int labelBufferLen = 0;
	static char * * textBuffer;
	static int textBufferCount = 0;
	char * cp;
	int col;
	int len;

	labelStr = gtkConvertInput( labelStr );
	len = strlen(labelStr)+1;
	if ( len > labelBufferLen ) {
		if ( labelBuffer )
			labelBuffer = realloc( labelBuffer, len );
		else
			labelBuffer = (char*)malloc( len );
		labelBufferLen = len;
	}
	if ( count > textBufferCount ) {
		if ( textBuffer )
			textBuffer = (char**)malloc( count * sizeof *textBuffer );
		else
			textBuffer = (char**)realloc( textBuffer, count * sizeof *textBuffer );
		textBufferCount = count;
	}

	strcpy( labelBuffer, labelStr );
	cp = labelBuffer;
	for ( col=0; cp && col<count; col++ ) {
		textBuffer[col] = cp;
		cp = strchr( cp, '\t' );
		if ( cp != NULL )
			*cp++ = '\0';
	}
	for ( ; col<count; col++ )
		textBuffer[col] = "";
	*texts = textBuffer;
}


EXPORT void wListSetValue(
		wList_p bl,
		const char * val )
{
	if (bl->list==NULL) abort();
	bl->recursion++;
	if (bl->type == B_DROPLIST) {
		bl->editted = TRUE;
		gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(bl->widget)->entry), val );
		if (bl->action) {
			bl->action( -1, val, 0, bl->data, NULL );
		}
	}
	bl->recursion--;
}


EXPORT wIndex_t wListFindValue(
		wList_p b,
		const char * val )
{
	GList * child;
	GtkObject * listItem;
	wListItem_p id_p;
	wIndex_t inx;

	if (b->list==NULL) abort();
	if (b->type == B_DROPLIST) {
		for ( child=GTK_LIST(b->list)->children,inx=0; child; child=child->next,inx++ ) {
			listItem = GTK_OBJECT(child->data);
			id_p = (wListItem_p)gtk_object_get_data(listItem, ListItemDataKey );
			if ( id_p && id_p->label && strcmp( id_p->label, val ) == 0 ) {
				return inx;
			}
		}
	} else {
		for ( inx=0; inx<b->count; inx++ ) {
			id_p = (wListItem_p)gtk_clist_get_row_data( GTK_CLIST(b->list), inx );
			if ( id_p && id_p->label && strcmp( id_p->label, val ) == 0 )
				return inx;
		}
	}
	return -1;
}

EXPORT wIndex_t wListGetIndex(
		wList_p b )		/* List */
/*
Returns the current selected list entry.
If <val> if '-1' then no entry is selected.
*/
{
	if (b->list == 0) abort();
	return b->last;
}

EXPORT wIndex_t wListGetValues(
		wList_p bl,
		char * labelStr,
		int labelSize,
		void * * listDataRet,
		void * * itemDataRet )

{
	wListItem_p id_p;
	wIndex_t inx = bl->last;
	const char * entry_value = "";
	void * item_data = NULL;

	if ( bl->list == 0 ) abort();
	if ( bl->type == B_DROPLIST && bl->editted ) {
		entry_value = gtk_entry_get_text( GTK_ENTRY(GTK_COMBO(bl->widget)->entry) );
		inx = bl->last = -1;
	} else {
		inx = bl->last;
		if (inx >= 0) {
			id_p = getListItem( bl, inx, NULL );
			if (id_p==NULL) {
				fprintf( stderr, "wListGetValues - id_p == NULL\n" );
				inx = -1;
			} else {
				entry_value = id_p->label;
				item_data = id_p->itemData;
			}
		}
	}
	if ( labelStr ) {
		strncpy( labelStr, entry_value, labelSize );
	}
	if ( listDataRet )
		*listDataRet = bl->data;
	if ( itemDataRet )
		*itemDataRet = item_data;
	return bl->last;
}


EXPORT wIndex_t wListGetCount(
		wList_p b )
{
	return b->count;
}


EXPORT void * wListGetItemContext(
		wList_p b,
		wIndex_t inx )
{
	wListItem_p id_p;
	if ( inx < 0 )
		return NULL;
	id_p = getListItem( b, inx, NULL );
	if ( id_p )
		return id_p->itemData;
	else
		return NULL;
}


EXPORT wBool_t wListGetItemSelected(
		wList_p b,
		wIndex_t inx )
{
	wListItem_p id_p;
	if ( inx < 0 )
		return FALSE;
	id_p = getListItem( b, inx, NULL );
	if ( id_p )
		return id_p->selected;
	else
		return FALSE;
}


EXPORT wIndex_t wListGetSelectedCount(
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

	// mark all items selected
	gtk_clist_select_all(GTK_CLIST(bl->list));

	// and synchronize the internal data structures
	wListGetCount(bl);
	for ( inx=0; inx<bl->count; inx++ ) {
		ldp = getListItem( bl, inx, NULL );

		ldp->selected = TRUE;
	}
}

EXPORT wBool_t wListSetValues(
		wList_p b,
		wIndex_t row,
		const char * labelStr,
		wIcon_p bm,
		void *itemData )

{
	wListItem_p id_p;
	GList * child;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap = NULL;
	char ** texts;
	int col;

	if (b->list == 0) abort();
	b->recursion++;
	id_p = getListItem( b, row, &child );
	if (id_p != NULL) {
		if ( b->type == B_DROPLIST ) {
			gtk_label_set( id_p->labelG, labelStr?gtkConvertInput(labelStr):"" );
			id_p->label = strdup( labelStr?labelStr:"" );
		} else {
			parseLabelStr( labelStr, b->colCnt, &texts );
			for ( col=0; col<b->colCnt; col++ )
				gtk_clist_set_text( GTK_CLIST(b->list), row, col, texts[col] );
			if ( bm ) {
				pixmap = gtkMakeIcon( b->widget, bm, &bitmap );
				gtk_clist_set_pixtext( GTK_CLIST(b->list), row, 0, texts[0], 5, pixmap, bitmap );
				gdk_pixmap_unref( pixmap );
				gdk_bitmap_unref( bitmap );
			}
		}
		id_p->itemData = itemData;
	}
	b->recursion--;
	return TRUE;
}


EXPORT void wListDelete(
		wList_p b,
		wIndex_t inx )

{
	wListItem_p id_p;
	GList * child;

	if (b->list == 0) abort();
	b->recursion++;
	if ( b->type == B_DROPLIST ) {
		id_p = getListItem( b, inx, &child );
		if (id_p != NULL) {
			gtk_container_remove( GTK_CONTAINER(b->list), child->data );
			b->count--;
		}
	} else {
		gtk_clist_remove( GTK_CLIST(b->list), inx );
		b->count--;
	}
	b->recursion--;
	return;
}


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


static void gtkDropListAddValue(
		wList_p b,
		wListItem_p id_p )
{
	GtkWidget * listItem;

	if ( id_p == NULL )
		return;
	id_p->labelG = (GtkLabel*)gtk_label_new( gtkConvertInput(id_p->label) );
	/*gtk_misc_set_alignment( GTK_MISC(id_p->labelG), 0.0, 0.5 );*/
	gtk_widget_show( GTK_WIDGET(id_p->labelG) );

	listItem = gtk_list_item_new();
	gtk_object_set_data( GTK_OBJECT(listItem), ListItemDataKey, id_p );
	gtk_container_add( GTK_CONTAINER(listItem), GTK_WIDGET(id_p->labelG) );
	gtk_misc_set_alignment( GTK_MISC(id_p->labelG), 0.0, 0.5 );
	gtk_container_add( GTK_CONTAINER(b->list), listItem );
	gtk_widget_show( listItem );
}


EXPORT wIndex_t wListAddValue(
		wList_p b,		/* List */
		const char * labelStr,	/* Entry name */
		wIcon_p bm,	/* Entry bitmap */
		void * itemData )	/* User context */
/*
Adds a entry to the list <b> with name <name>.
If list is created with 'BL_
*/
{
	wListItem_p id_p;
	GdkPixmap * pixmap = NULL;
	GdkBitmap * bitmap;
	static char ** texts;
	GtkAdjustment *adj;

	if (b->list == 0) abort();
	b->recursion++;
	id_p = (wListItem_p)malloc( sizeof *id_p );
	memset( id_p, 0, sizeof *id_p );
	id_p->itemData = itemData;
	id_p->active = TRUE;
	if ( labelStr == NULL )
		labelStr = "";
	id_p->label = strdup( labelStr );
	id_p->listP = b;
	if ( b->type == B_DROPLIST ) {
		gtkDropListAddValue( b, id_p );
	} else {
		parseLabelStr( labelStr, b->colCnt, &texts );
		gtk_clist_append( GTK_CLIST(b->list), texts );

		/*
		 * this has taken forever to find out: the adjustment has to be notified
		 * about the list change by the program. So we need to get the current alignment.
		 * increment the upper value and then inform the scrolled window about the update.
		 * The upper value is increased only if the current value is smaller than the size
		 * of the list box.
		 */

		adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(b->widget));

		if( adj->upper < adj->step_increment * (b->count+1)) {
			adj->upper += adj->step_increment;
			gtk_adjustment_changed( adj );
		}
		if ( bm ) {
			pixmap = gtkMakeIcon( b->widget, bm, &bitmap );
			gtk_clist_set_pixtext( GTK_CLIST(b->list), b->count, 0, texts[0], 5, pixmap, bitmap );
			gdk_pixmap_unref( pixmap );
			gdk_bitmap_unref( bitmap );
		}
		gtk_clist_set_row_data( GTK_CLIST(b->list), b->count, id_p );
	}

	b->count++;
	b->recursion--;
	if ( b->count == 1 ) {
		b->last = 0;
	}
	return b->count-1;
}


void wListSetSize( wList_p bl, wPos_t w, wPos_t h )
{
	/*gtk_widget_set_usize( bl->list, w, h );*/
	if (bl->type == B_DROPLIST) {
		/*gtk_widget_set_usize( GTK_COMBO(bl->widget)->entry, w, -1 );
		gtk_widget_set_usize( GTK_COMBO(bl->widget)->list, w, -1 );*/
#ifndef GTK1
		gtk_widget_set_size_request( bl->widget, w, -1 );
#else
		gtk_widget_set_usize( bl->widget, w, -1 );
#endif
	} else {
#ifndef GTK1
		gtk_widget_set_size_request( bl->widget, w, h );
#else
		gtk_widget_set_usize( bl->widget, w, h );
#endif
	}
	bl->w = w;
	bl->h = h;
}




EXPORT void wListSetActive(
		wList_p b,		/* List */
		int inx,		/* Index */
		wBool_t active )	/* Command */
/*
*/
{
	wListItem_p id_p;
	GList * child;

	if (b->list == 0) abort();
	id_p = getListItem( b, inx, &child );
	if (id_p == NULL)
		return;
	gtk_widget_set_sensitive( GTK_WIDGET(child->data), active );
}


EXPORT void wListSetEditable(
		wList_p b,
		wBool_t editable )
{
	b->editable = editable;
	if ( b->type == B_DROPLIST )
		gtk_widget_set_sensitive( GTK_WIDGET(GTK_COMBO(b->widget)->entry), b->editable );
}


static int selectCList(
		GtkWidget * clist,
		int row,
		int col,
		GdkEventButton* event,
		gpointer data )
{
	wList_p bl = (wList_p)data;
	wListItem_p id_p;

	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(0);
	}
	wFlush();
	if (bl->recursion)
		return 0;
	id_p = gtk_clist_get_row_data( GTK_CLIST(clist), row );
	if ( id_p == NULL ) return 1;
	bl->editted = FALSE;
	if ( (bl->option&BL_MANY)==0 && bl->last == row )
		return 1;
	bl->last = row;
	id_p->selected = TRUE;
	if (bl->valueP)
		*bl->valueP = row;
	if (bl->action)
		bl->action( row, id_p->label, 1, bl->data, id_p->itemData );
	return 1;
}


static int unselectCList(
		GtkWidget * clist,
		int row,
		int col,
		GdkEventButton* event,
		gpointer data )
{
	wList_p bl = (wList_p)data;
	wListItem_p id_p;

	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(0);
	}
	wFlush();
	if (bl->recursion)
		return 0;
	id_p = gtk_clist_get_row_data( GTK_CLIST(clist), row );
	if ( id_p == NULL ) return 1;
	id_p->selected = FALSE;
	if (bl->action)
		bl->action( row, id_p->label, 2, bl->data, id_p->itemData );
	return 1;
}


static int resizeColumnCList(
		GtkWidget * clist,
		int col,
		int width,
		gpointer data )
{
	wList_p bl = (wList_p)data;

	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(0);
	}
	wFlush();
	if (bl->recursion)
		return 0;
	if ( col >= 0 && col < bl->colCnt )
		bl->colWidths[col] = width;
	return 0;
}



static int DropListSelectChild(
		GtkWidget * list,
		GtkWidget * listItem,
		gpointer data )
{
	wList_p bl = (wList_p)data;
	wListItem_p id_p=NULL;
	wIndex_t inx;
	GList * child;

	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(0);
	}
#ifdef LATER
	printf( "DropListSelectChild %p %p %p\n", list, listItem, data );
	printf( "  b: recurs=%d widget=%p\n", bl->recursion, bl->list );
#endif
	if (bl->recursion)
		return 0;
	wFlush();
	id_p = gtk_object_get_data(GTK_OBJECT(listItem), ListItemDataKey );
	if ( id_p == NULL ) {
		fprintf( stderr, "  id_p = NULL\n");
		return 0;
	}
#ifdef LATER
	printf( "  id_p = %s %lx, %d %d\n", id_p->label, (long)id_p->itemData, id_p->active, id_p->selected );
#endif
	if ( bl->type == B_DROPLIST && bl->editable ) {
		if ( bl->editted == FALSE )
			return 0;
		gtkSetTrigger( NULL, NULL );
	}
	bl->editted = FALSE;
	for ( inx=0,child=GTK_LIST(bl->list)->children,inx=0; child&&child->data!=listItem; child=child->next ) inx++;
	if ( bl->last == inx )
		return 1;
	bl->last = inx;
	if (bl->valueP)
		*bl->valueP = inx;
	if (id_p && bl->action)
		bl->action( (wIndex_t)inx, id_p->label, 1, bl->data, id_p->itemData );
	gtkSetTrigger( NULL, NULL );
	return 1;
}


#ifdef LATER
static int DropListSelectionChanged(
		GtkWidget * list,
		gpointer data )
{
	wList_p bl = (wList_p)data;
	wListItem_p id_p=NULL;
	GList * child;
	GList * dlist;
	wIndex_t inx;
	GtkObject * listItem;
	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(0);
	}
	if (bl->recursion)
		return 0;
	wFlush();
	if ( bl->type == B_DROPLIST && bl->editable ) {
		if ( bl->editted == FALSE )
			return 0;
		gtkSetTrigger( NULL, NULL );
	}

	dlist = GTK_LIST(bl->list)->selection;
	if (dlist == NULL) {
		return 0;
	}
	inx = 0;
	for ( child=GTK_LIST(bl->list)->children,inx=0; child&&child->data!=dlist->data; child=child->next ) inx++;
	while (dlist) {
		listItem = GTK_OBJECT(dlist->data);
		id_p = gtk_object_get_data(listItem, ListItemDataKey );
		printf( "DropListSelectionChanged: id_p = %s %lx\n", id_p->label, (long)id_p->itemData );
		dlist = dlist->next;
	}
	return 0;
#ifdef LATER
	bl->editted = FALSE;
	if ( bl->last == inx )
		return 1;
	bl->last = inx;
	if (bl->valueP)
		*bl->valueP = inx;
	if (id_p && bl->action)
		bl->action( inx, id_p->label, 1, bl->data, id_p->itemData );
	gtkSetTrigger( NULL, NULL );
	return 1;
#endif
}

#endif


static void triggerDListEntry(
		wControl_p b )
{
	wList_p bl = (wList_p)b;
	const char * entry_value;

	if (bl == 0)
		return;
	if (bl->widget == 0) abort();
	entry_value = gtk_entry_get_text( GTK_ENTRY(GTK_COMBO(bl->widget)->entry) );
	if (entry_value == NULL) return;
	if (debugWindow >= 2) printf("triggerListEntry: %s text = %s\n", bl->labelStr?bl->labelStr:"No label", entry_value );
	if (bl->action) {
		bl->recursion++;
		bl->action( -1, entry_value, 0, bl->data, NULL );
		bl->recursion--;
	}
	gtkSetTrigger( NULL, NULL );
	return;
}


static void updateDListEntry(
		GtkEntry * widget,
		wList_p bl )
{
	const char *entry_value;
	if (bl == 0)
		return;
	if (bl->recursion)
		return;
	if (!bl->editable)
		return;
	entry_value = gtk_entry_get_text( GTK_ENTRY(GTK_COMBO(bl->widget)->entry) );
	bl->editted = TRUE;
	if (bl->valueP != NULL)
		*bl->valueP = -1;
	bl->last = -1;
	if (bl->action)
		gtkSetTrigger( (wControl_p)bl, triggerDListEntry );
	return;
}



#ifdef LATER
EXPORT wList_p wListCreate(
		wWin_p	parent,		/* Parent window */
		wPos_t	x,		/* X-position */
		wPos_t	y,		/* Y-position */
		const char 	* helpStr,	/* Help string */
		const char	* labelStr,	/* Label */
		long	option,		/* Options */
		long	number,		/* Number of displayed entries */
		wPos_t	width,		/* Width of list */
		long	*valueP,	/* Selected index */
		wListCallBack_p action,	/* Callback */
		void 	*data )		/* Context */
/*
*/
{
	wList_p b;

	b = (wList_p)gtkAlloc( parent, B_LIST, x, y, labelStr, sizeof *b, data );
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
	gtkComputePos( (wControl_p)b );

	b->list = (GtkWidget*)gtk_clist_new(1);
	if (b->list == 0) abort();
	b->widget = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (b->widget),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	/*gtk_container_add( GTK_CONTAINER(b->widget), b->list );*/
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(b->widget), b->list );
	if (width == 0)
		width = 100;
	gtk_clist_set_column_width( GTK_CLIST(b->list), 0, width );
#ifndef GTK1
	gtk_widget_set_size_request( b->widget, width, (number+1)*ROW_HEIGHT );
#else
	gtk_widget_set_usize( b->widget, width, (number+1)*ROW_HEIGHT );
#endif
	gtk_signal_connect( GTK_OBJECT(b->list), "select_row", GTK_SIGNAL_FUNC(selectCList), b );
	gtk_signal_connect( GTK_OBJECT(b->list), "unselect_row", GTK_SIGNAL_FUNC(unselectCList), b );
	gtk_list_set_selection_mode( GTK_LIST(b->list), (option&BL_MANY)?GTK_SELECTION_MULTIPLE:GTK_SELECTION_BROWSE );
/*	gtk_container_set_focus_vadjustment (GTK_CONTAINER (b->list),
				gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (b->widget))); */
	gtk_widget_show( b->list );

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	gtkControlGetSize( (wControl_p)b );
	if (labelStr)
		b->labelW = gtkAddLabel( (wControl_p)b, labelStr );
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	gtkAddHelpString( b->widget, helpStr );
	return b;
}
#endif

/** Create a drop down list. The drop down is created and intialized with the supplied values.
 *
 *		\param IN parent Parent window
 *		\param IN x, X-position
 *		\param IN y	 Y-position
 *		\param IN helpStr Help string
 *		\param IN labelStr Label
 *		\param IN option Options
 *		\param IN number Number of displayed entries
 *		\param IN width Width
 *		\param IN valueP Selected index
 *		\param IN action Callback
 *		\param IN data Context
 */

EXPORT wList_p wDropListCreate(
		wWin_p	parent,		/* Parent window */
		wPos_t	x,		/* X-position */
		wPos_t	y,		/* Y-position */
		const char 	* helpStr,	/* Help string */
		const char	* labelStr,	/* Label */
		long	option,		/* Options */
		long	number,		/* Number of displayed entries */
		wPos_t	width,		/* Width */
		long	*valueP,	/* Selected index */
		wListCallBack_p action,	/* Callback */
		void 	*data )		/* Context */
/*
*/
{
	wList_p b;
	b = (wList_p)gtkAlloc( parent, B_DROPLIST, x, y, labelStr, sizeof *b, data );
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
	gtkComputePos( (wControl_p)b );

	b->widget = (GtkWidget*)gtk_combo_new();
	if (b->widget == 0) abort();
	b->list = GTK_COMBO(b->widget)->list;
#ifdef LATER
	gtk_signal_connect( GTK_OBJECT(b->list), "selection_changed", GTK_SIGNAL_FUNC(DropListSelectionChanged), b );
#endif
	gtk_signal_connect( GTK_OBJECT(b->list), "select_child", GTK_SIGNAL_FUNC(DropListSelectChild), b );
	if (width == 0)
		width = 100;
#ifndef GTK1
	gtk_widget_set_size_request( b->widget, width, -1 );
	gtk_widget_set_size_request( GTK_COMBO(b->widget)->entry, width, -1 );
#else
	gtk_widget_set_usize( b->widget, width, -1 );
	gtk_widget_set_usize( GTK_COMBO(b->widget)->entry, width, -1 );
#endif

	gtk_signal_connect( GTK_OBJECT(GTK_COMBO(b->widget)->entry), "changed", GTK_SIGNAL_FUNC(updateDListEntry), b );
	if ( (option&BL_EDITABLE) == 0 )
		gtk_widget_set_sensitive( GTK_WIDGET(GTK_COMBO(b->widget)->entry), FALSE );
	else {
		b->editable = TRUE;
	}

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	gtkControlGetSize( (wControl_p)b );
	if (labelStr)
		b->labelW = gtkAddLabel( (wControl_p)b, labelStr );
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	gtkAddHelpString( b->widget, helpStr );
	return b;

}


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
	return wListCreate( parent, x, y, helpStr, labelStr, option, number, width, 0, NULL, NULL, NULL, valueP, action, data );
#ifdef LATER
	wList_p b;

	b = (wList_p)gtkAlloc( parent, B_LIST, x, y, labelStr, sizeof *b, data );
	b->option = option;
	b->number = number;
	b->count = 0;
	b->last = -1;
	b->valueP = valueP;
	b->action = action;
	b->listX = b->realX;
	gtkComputePos( (wControl_p)b );

	b->widget = (GtkWidget*)gtk_combo_new();
	if (b->widget == 0) abort();
	if (width == 0)
		width = 100;
	/*gtk_clist_set_column_width( GTK_CLIST(b->widget), 0, width );*/
#ifndef GTK1
	gtk_widget_set_size_request( b->widget, width, -1 );
#else
	gtk_widget_set_usize( b->widget, width, -1 );
#endif

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	gtkControlGetSize( (wControl_p)b );
	if (labelStr)
		b->labelW = gtkAddLabel( (wControl_p)b, labelStr );
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	gtkAddHelpString( b->widget, helpStr );
	return b;
#endif
}



EXPORT wList_p wListCreate(
		wWin_p	parent,		/* Parent window */
		wPos_t	x,		/* X-position */
		wPos_t	y,		/* Y-position */
		const char 	* helpStr,	/* Help string */
		const char	* labelStr,	/* Label */
		long	option,		/* Options */
		long	number,		/* Number of displayed entries */
		wPos_t	width,		/* Width of list */
		int	colCnt,		/* Number of columns */
		wPos_t	* colWidths,	/* Width of columns */
		wBool_t * colRightJust,	/* justification of columns */
		const char 	** colTitles,	/* Title of columns */
		long	*valueP,	/* Selected index */
		wListCallBack_p action,	/* Callback */
		void 	*data )		/* Context */
/*
*/
{
	wList_p bl;
	long col;
	static wPos_t zeroPos = 0;

	bl = (wList_p)gtkAlloc( parent, B_LIST, x, y, labelStr, sizeof *bl, data );
	bl->option = option;
	bl->number = number;
	bl->count = 0;
	bl->last = -1;
	bl->valueP = valueP;
	bl->action = action;
	bl->listX = bl->realX;

	if ( colCnt <= 0 ) {
		colCnt = 1;
		colWidths = &zeroPos;
	}
	bl->colCnt = colCnt;
	bl->colWidths = (wPos_t*)malloc( colCnt * sizeof *(wPos_t*)0 );
	memcpy( bl->colWidths, colWidths, colCnt * sizeof *(wPos_t*)0 );

	gtkComputePos( (wControl_p)bl );

	bl->list = (GtkWidget*)gtk_clist_new( bl->colCnt );
	if (bl->list == 0) abort();
	if (colTitles)
	{
		for (col = 0; col < colCnt; col++)
			gtk_clist_set_column_title(GTK_CLIST(bl->list), col, _(((char*)colTitles[col])));
		gtk_clist_column_titles_show(GTK_CLIST(bl->list));
	}

	bl->widget = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (bl->widget),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	/* gtk_container_add( GTK_CONTAINER(bl->widget), bl->list ); */
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(bl->widget), bl->list );
	if (width == 0)
		width = 100;
	for ( col=0; col<colCnt; col++ ) {
		gtk_clist_set_column_auto_resize( GTK_CLIST(bl->list), col, TRUE );
		gtk_clist_set_column_resizeable( GTK_CLIST(bl->list), col, TRUE );
		gtk_clist_set_column_justification( GTK_CLIST(bl->list), col,
				(colRightJust==NULL||colRightJust[col]==FALSE)?GTK_JUSTIFY_LEFT:GTK_JUSTIFY_RIGHT );
		gtk_clist_set_column_width( GTK_CLIST(bl->list), col, bl->colWidths[col] );
	}
#ifndef GTK1
	gtk_widget_set_size_request( bl->widget, width, (number+1)*ROW_HEIGHT );
#else
	gtk_widget_set_usize( bl->widget, width, (number+1)*ROW_HEIGHT );
#endif
	gtk_signal_connect( GTK_OBJECT(bl->list), "select_row", GTK_SIGNAL_FUNC(selectCList), bl );
	gtk_signal_connect( GTK_OBJECT(bl->list), "unselect_row", GTK_SIGNAL_FUNC(unselectCList), bl );
	gtk_signal_connect( GTK_OBJECT(bl->list), "resize_column", GTK_SIGNAL_FUNC(resizeColumnCList), bl );
	gtk_clist_set_selection_mode( GTK_CLIST(bl->list), (option&BL_MANY)?GTK_SELECTION_MULTIPLE:GTK_SELECTION_BROWSE );
	gtk_container_set_focus_vadjustment (GTK_CONTAINER (bl->list),
				gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (bl->widget)));

	gtk_widget_show( bl->list );

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), bl->widget, bl->realX, bl->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), bl->widget );
	gtk_widget_set_uposition( bl->widget, bl->realX, bl->realY );
#endif
	gtkControlGetSize( (wControl_p)bl );
	if (labelStr)
		bl->labelW = gtkAddLabel( (wControl_p)bl, labelStr );
	gtk_widget_show( bl->widget );
	gtkAddButton( (wControl_p)bl );
	gtkAddHelpString( bl->widget, helpStr );
	return bl;
}
