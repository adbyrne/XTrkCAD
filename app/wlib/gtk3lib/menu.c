/** \file menu.c
 * Menu creation and handling.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2012 Martin Fischer
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
#include <ctype.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gtkint.h"
#include "i18n.h"

#define WLISTITEM	"wListItem"		/**< id for object data */

/*
 *****************************************************************************
 *
 * Menus
 *
 *****************************************************************************
 */

typedef enum { M_MENU, M_SEPARATOR, M_PUSH, M_LIST, M_LISTITEM, M_TOGGLE, M_RADIO } mtype_e;
typedef enum { MM_BUTT, MM_MENU, MM_BAR, MM_POPUP } mmtype_e;

typedef struct{	mtype_e mtype;			/**< menu entry type */
				GtkWidget *menu_item;
				wMenu_p parentMenu;
				int recursion; 			/**< recursion counter */
		} MOBJ_COMMON; 					/**< menu item specific data */
							
	
struct wMenuItem_t {
	WOBJ_COMMON
	MOBJ_COMMON m;
	};

typedef struct wMenuItem_t * wMenuItem_p;

// a few macros to make access to members easier	
//#define PTR2M( ptr ) ((ptr)->m)	
#define MMENUITEM( ptr ) 	(((ptr)->m).menu_item)	
#define MPARENT( ptr ) 	(((ptr)->m).parentMenu)
#define MITEMTYPE( ptr )	(((ptr)->m).mtype)
#define MRECURSION( ptr ) (((ptr)->m).recursion)	
	
	
struct wMenu_t {
	WOBJ_COMMON
	MOBJ_COMMON m;
	mmtype_e mmtype;
	wMenuItem_p first, last;
	GtkWidget * menu;
	GSList *radioGroup;			/* radio button group */
	wMenuTraceCallBack_p traceFunc;
	void * traceData;
	GtkLabel * labelG;
	GtkWidget * imageG;
	};

struct wMenuPush_t {
	WOBJ_COMMON
	MOBJ_COMMON m;
	wMenuCallBack_p action;
	wBool_t enabled;
	};

struct wMenuRadio_t {
	WOBJ_COMMON
	MOBJ_COMMON m;
	wMenuCallBack_p action;
	wBool_t enabled;
	};

struct wMenuList_t {
	WOBJ_COMMON
	MOBJ_COMMON m;
	int max;
	int count;
	wMenuListCallBack_p action;
	};

struct wMenuListItem_t {
	WOBJ_COMMON
	MOBJ_COMMON m;
	wMenuList_p mlist;
	};

typedef struct wMenuListItem_t * wMenuListItem_p;
	
struct wMenuToggle_t {
	WOBJ_COMMON
	MOBJ_COMMON m;
	wMenuToggleCallBack_p action;
	wBool_t enabled;
	wBool_t set;
	};


/*-----------------------------------------------------------------*/
	
/**
 * Handle activate event for menu items.
 *
 * \param widget IN widget that emitted the signal
 * \param value  IN application data
 * \return 
 */
	
static void pushMenuItem(
	GtkWidget * widget,
	gpointer value )
{
	wMenuItem_p m = (wMenuItem_p)value;
	wMenuToggle_p mt;

	if (MRECURSION( m ))
		return;

	switch MITEMTYPE( m ) {
	case M_PUSH:
		((wMenuPush_p)m)->action( ((wMenuPush_p)m)->data );
		break;
	case M_TOGGLE:
		mt = (wMenuToggle_p)m;
		wMenuToggleSet( mt, !mt->set );
		mt->action( mt->set, mt->data );
		break;
	case M_RADIO:
		/* NOTE: action is only called when radio button is activated, not when deactivated */
		if( gtk_check_menu_item_get_active((GtkCheckMenuItem *)widget ) == TRUE )
			((wMenuRadio_p)m)->action( ((wMenuRadio_p)m)->data );
		break;	
	case M_MENU:
		return;
	default:
		/*fprintf(stderr," Oops menu\n");*/
		return;
	}
	if( MPARENT(m)->traceFunc ) {
		MPARENT(m)->traceFunc( MPARENT( m ), m->labelStr,  MPARENT(m)->traceData );
	}
} 

/**
 * Create a new menu element, add to the parent menu and to help
 *
 * \param m 		IN parent menu
 * \param mtype 	IN type of new entry
 * \param helpStr 	IN help topic 
 * \param labelStr 	IN display label
 * \param size 		IN size of additional data?
 * \return    the newly created menu element
 */

static wMenuItem_p createMenuItem(
	wMenu_p m,
	mtype_e mtype,
	const char * helpStr,
	const char * labelStr,
	int size )
{
	wMenuItem_p mi;
	mi = (wMenuItem_p)wlibAlloc( NULL, B_MENUITEM, 0, 0, labelStr, size, NULL );
	MITEMTYPE( mi )= mtype;
	
	switch ( mtype ) {
	case M_LIST:
		MMENUITEM( mi ) = gtk_menu_item_new_with_mnemonic(wlibConvertInput(mi->labelStr)); // NULL;  //PTR2M(m).menu_item
		break;
	case M_SEPARATOR:
		MMENUITEM( mi ) = gtk_separator_menu_item_new();
		break;
	case M_TOGGLE:
		MMENUITEM( mi ) = gtk_check_menu_item_new_with_mnemonic(wlibConvertInput(mi->labelStr));
		break;
	case M_RADIO:
		MMENUITEM( mi ) = gtk_radio_menu_item_new_with_mnemonic(m->radioGroup, wlibConvertInput(mi->labelStr));
		m->radioGroup = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (MMENUITEM( mi )));
		break;			
	default:
		MMENUITEM( mi ) = gtk_menu_item_new_with_mnemonic(wlibConvertInput(mi->labelStr));
		break;
	}
	if (MMENUITEM( mi )) {
		if (m)
			gtk_menu_shell_append( (GtkMenuShell *)(m->menu), MMENUITEM( mi ) );

		g_signal_connect( MMENUITEM( mi ), "activate",
				G_CALLBACK(pushMenuItem), mi );
 		gtk_widget_show(MMENUITEM( mi ));
	}
	
	// this is  a link list of all menu items belonging to a specific menu
	// is used in automatic processing (macro)??
	if (m) {
		if (m->first == NULL) {
			m->first = mi;
		} else {
			m->last->next = (wControl_p)mi;
		}
		m->last = mi;
	}
	mi->next = NULL;
	
	
	if (helpStr != NULL) {
		wlibAddHelpString( MMENUITEM( mi ), helpStr );
	}
	MPARENT( mi ) = m;
	return mi;
}

/**
 * Add a accelerator key to a widget
 * 
 * @param w         IN unused(?)
 * @param menu      IN unused(?)
 * @param menu_item IN owning widget
 * @param acclKey   IN the accelerator key
 */
static void setAcclKey( wWin_p w, GtkWidget * menu, GtkWidget * menu_item, int acclKey )
{
	int mask;
	static GtkAccelGroup * accel_alpha_group = NULL;
	static GtkAccelGroup * accel_nonalpha_group = NULL;
	guint oldmods;


	if (accel_alpha_group == NULL) {
		accel_alpha_group = gtk_accel_group_new();
		/*gtk_accel_group_set_mod_mask( accel_group, GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_MOD1_MASK );*/
		gtk_window_add_accel_group(GTK_WINDOW(gtkMainW->gtkwin), accel_alpha_group );
	}
	if (accel_nonalpha_group == NULL) {
		oldmods = gtk_accelerator_get_default_mod_mask();
		gtk_accelerator_set_default_mod_mask( GDK_CONTROL_MASK | GDK_MOD1_MASK );
		accel_nonalpha_group = gtk_accel_group_new();
		/*gtk_accel_group_set_mod_mask( accel_group, GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_MOD1_MASK );*/
		gtk_window_add_accel_group(GTK_WINDOW(gtkMainW->gtkwin), accel_nonalpha_group );
		gtk_accelerator_set_default_mod_mask( oldmods );
	}
 
	mask = 0;
	if (acclKey) {
		
		if (acclKey&WALT) {
			mask |= GDK_MOD1_MASK;
		}
		if (acclKey&WSHIFT) {
			mask |= GDK_SHIFT_MASK;
			switch ( (acclKey&0xFF) ) {
			case '0': acclKey += ')'-'0'; break;
			case '1': acclKey += '!'-'1'; break;
			case '2': acclKey += '@'-'2'; break;
			case '3': acclKey += '#'-'3'; break;
			case '4': acclKey += '$'-'4'; break;
			case '5': acclKey += '%'-'5'; break;
			case '6': acclKey += '^'-'6'; break;
			case '7': acclKey += '&'-'7'; break;
			case '8': acclKey += '*'-'8'; break;
			case '9': acclKey += '('-'9'; break;
			case '`': acclKey += '~'-'`'; break;
			case '-': acclKey += '_'-'-'; break;
			case '=': acclKey += '+'-'='; break;
			case '\\': acclKey += '|'-'\\'; break;
			case '[': acclKey += '{'-'['; break;
			case ']': acclKey += '}'-']'; break;
			case ';': acclKey += ':'-';'; break;
			case '\'': acclKey += '"'-'\''; break;
			case ',': acclKey += '<'-','; break;
			case '.': acclKey += '>'-'.'; break;
			case '/': acclKey += '?'-'/'; break;
			default: break;
			}
		}
		if (acclKey&WCTL) {
			mask |= GDK_CONTROL_MASK;
		}
		gtk_widget_add_accelerator( menu_item, "activate",
			(isalpha(acclKey&0xFF)?accel_alpha_group:accel_nonalpha_group),
			toupper(acclKey&0xFF), mask, GTK_ACCEL_VISIBLE|GTK_ACCEL_LOCKED );
	}
}

/*-----------------------------------------------------------------*/
/**
 * Create a radio button as a menu entry 
 *
 * \param m 		IN menu to be extended
 * \param helpStr 	IN reference into help
 * \param labelStr 	IN text for entry
 * \param acclKey 	IN accelerator key to add
 * \param action 	IN callback function 
 * \param data 		IN application data
 * \param helpStr 	IN
 * \return menu entry
 */

wMenuRadio_p wMenuRadioCreate(
	wMenu_p m, 
	const char * helpStr,
	const char * labelStr,
	long acclKey,
	wMenuCallBack_p action,
	void 	*data )
{
	wMenuRadio_p mi;

	mi = (wMenuRadio_p)createMenuItem( m, M_RADIO, helpStr, labelStr, sizeof *mi );
	//~ if (m->mmtype == MM_POPUP && !testMenuPopup)
		//~ return mi;
	setAcclKey( m->parent, m->menu, MMENUITEM( mi ), acclKey );
	mi->action = action;
	mi->data = data;
	mi->enabled = TRUE;
	return mi;
}

/**
 * Set radio button active
 *
 * \param mi 		IN menu entry for radio button
 * \return
 */

void wMenuRadioSetActive( 
	wMenuRadio_p mi )
{
	gtk_check_menu_item_set_active( (GtkCheckMenuItem *)MMENUITEM(mi), TRUE ); 
}	 		

/*-----------------------------------------------------------------*/

/**
 * Create a menu entry 
 *
 * \param m 		IN menu to be extended
 * \param helpStr 	IN reference into help
 * \param labelStr 	IN text for entry
 * \param acclKey 	IN acceleratoor key to add
 * \param action 	IN callback function 
 * \param data 		IN application data
 * \return menu entry
 */

wMenuPush_p wMenuPushCreate(
	wMenu_p m, 
	const char * helpStr,
	const char * labelStr,
	long acclKey,
	wMenuCallBack_p action,
	void 	*data )
{
	wMenuPush_p mi;

	mi = (wMenuPush_p)createMenuItem( m, M_PUSH, helpStr, labelStr, sizeof( struct wMenuPush_t ));

	setAcclKey( m->parent, m->menu, MMENUITEM( mi ), acclKey );

	mi->action = action;
	mi->data = data;
	mi->enabled = TRUE;
	return mi;
}

/**
 * Enable menu entry
 *
 * \param mi 		IN menu entry
 * \param enable 	IN new state 
 * \return
 */

void wMenuPushEnable(
	wMenuPush_p mi,
	wBool_t enable )
{
	mi->enabled = enable;
	gtk_widget_set_sensitive( GTK_WIDGET(MMENUITEM( mi )), enable );
}


/*-----------------------------------------------------------------*/
/**
 * Create a submenu
 *
 * \param m 		IN menu to be extended
 * \param helpStr 	IN reference into help
 * \param labelStr 	IN text for entry
 * \return menu entry
 */

wMenu_p wMenuMenuCreate(
	wMenu_p m, 
	const char * helpStr,
	const char * labelStr )
{
	wMenu_p mi;
	mi = (wMenu_p)createMenuItem( m, M_MENU, helpStr, labelStr, sizeof( struct wMenu_t ));
	mi->mmtype = MM_MENU;
	mi->menu = gtk_menu_new();

	gtk_menu_item_set_submenu( GTK_MENU_ITEM(MMENUITEM( mi )), mi->menu );
	return mi;
}


/*-----------------------------------------------------------------*/
/**
 * Create a menu separator
 *
 * \param mi 		IN menu entry
 * \return
 */

void wMenuSeparatorCreate(
	wMenu_p m ) 
{
	createMenuItem( m, M_SEPARATOR, NULL, "", sizeof( struct wMenuItem_t ));
}


/*-----------------------------------------------------------------*/

/**
 * Find the start of a menu item list in a menu
  *
 * \param ml 		IN menu list to be searched
 * \param pChildren OUT list of children in menu container
 * \return -1 if not found, index in children list otherwise
 */

int getMlistOrigin( wMenuList_p ml, GList **pChildren )
{
	GtkWidget *mi;
	int count = 0;
	int found = -1;
	GtkWidget *mitem = MMENUITEM( ml );
	
	*pChildren = gtk_container_get_children( GTK_CONTAINER( MPARENT( ml )->menu ));
	if( !*pChildren )
		return( -1 );
	
	while( (mi = g_list_nth_data( *pChildren, count ))) {
		if( mi == mitem ) {
			found = TRUE;
			break;
		}	
		else {
			count++;
		}	
	}
	
	if( found )
		return( count );
	else 
		return( -1 );
}

/**
 * Signal handler for clicking onto a menu list item. 
 * Parameters are the GtkWidget as expected and the pointer to the MenuListItem
 *
 * \param widget IN the GtkWidget
 * \param value  IN the menu list item 
 * \return
 */

static void pushMenuList(
	GtkWidget * widget,
	gpointer value )
{
	// pointer to the list item
	wMenuListItem_p ml = (wMenuListItem_p)value;

	if (MRECURSION( ml ))
		return;
	
	if (ml->mlist->count <= 0) {
		// this should never happen
		fprintf( stderr, "pushMenuItem: empty list\n" );
		return;
	}
	// this is the applications callback routine
	if (ml->mlist->action) {
		const char * itemLabel;

		itemLabel = gtk_menu_item_get_label( GTK_MENU_ITEM( widget ));
		
		ml->mlist->action( 0, itemLabel, ml->data );	
		return;
	}
	fprintf( stderr, "pushMenuItem: item (%lx) not found\n", (long)widget );
}

/**
 * Create a list menu entry 
 *
 * \param m 		IN menu to be extended
 * \param helpStr 	IN reference into help
 * \param max 		IN maximum number of elements
 * \param action 	IN callback function 
 * \return menu entry
 */

wMenuList_p wMenuListCreate(
	wMenu_p m, 
	const char * helpStr,
	int max,
	wMenuListCallBack_p action )
{
	wMenuList_p mi;
	mi = (wMenuList_p)createMenuItem( m, M_LIST, NULL, _("<Empty List>"), sizeof( struct wMenuList_t ));
	gtk_widget_set_sensitive( GTK_WIDGET(MMENUITEM( mi )), FALSE );
	mi->next = NULL;
	mi->count = 0;
	mi->max = max;
	MPARENT( mi ) = m;
	mi->action = action;
	return (wMenuList_p)mi;
}


/**
 * Add a new item to a list of menu entries
 * The placeholder for the list is looked up. Then the new item is added immediately 
 * behind it. In case the maximum number of items is reached the last item is removed.
 *
 * \param ml 		IN handle for the menu list - the placeholder item
 * \param index 	IN currently ignored
 * \param labelStr 	IN the menu label for the new item 
 * \param data 		IN application data for the new item
 * \return    
 */

void wMenuListAdd(
	wMenuList_p ml,
	int index,
	const char * labelStr,
	const void * data )
{
	int i = 0;
	GList * children;

	i = getMlistOrigin( ml, &children );
	
	if( i > -1 ) {
		wMenuListItem_p mi;

		// we're adding an item, so hide the default placeholder
		gtk_widget_hide( MMENUITEM( ml ));
		
		// delete an earlier entry with the same label
		wMenuListDelete( ml, labelStr );		

		// a new item
		ml->count ++;
		
		// is there a maximum number of items set and reached with the new item?
		if(( ml->max != -1 ) && ( ml->count > ml-> max )) {
			wMenuListItem_p mold;
			GtkWidget * item;
	
			// get the last item in the list
			item = g_list_nth_data( children, i + ml->max );
			// get the pointer to the data structure
			mold = g_object_get_data( G_OBJECT( item ), WLISTITEM );
			// kill the menu entry
			gtk_widget_destroy( item );
			// free the data 
			free( (void *)mold->labelStr );
			free( (void *)mold );

			ml->count--;
		}	
		
		// create the new menu item and initialize the data fields
		mi = (wMenuListItem_p)wlibAlloc( NULL, B_MENUITEM, 0, 0, labelStr, sizeof( struct wMenuListItem_t ), NULL );
		MITEMTYPE( mi ) = M_LISTITEM;
		MMENUITEM( mi ) = gtk_menu_item_new_with_label(wlibConvertInput(mi->labelStr));
		mi->data = (void *)data;
		mi->mlist = ml;
		g_object_set_data( G_OBJECT(MMENUITEM( mi )), WLISTITEM,  mi );
		
		// add the item to the menu
		gtk_menu_shell_insert((GtkMenuShell *)(MPARENT( ml )->menu), MMENUITEM( mi ), i + 1 );
		g_signal_connect( MMENUITEM( mi ), "activate", G_CALLBACK(pushMenuList), mi );
	
		gtk_widget_show(MMENUITEM( mi ));
	}
	
	if( children )
		g_list_free( children );
}

/**
 * Remove the menu entry identified by a given label.
 *
 * \param ml IN menu list
 * \param labelStr IN label string of item 
 */

void wMenuListDelete(
	wMenuList_p ml,
	const char * labelStr )
{
	int i;
	int found = FALSE;
	GList * children;

	// find the placeholder for the list in the menu
	i = getMlistOrigin( ml, &children );
	
	if( i > -1 ) {
		int origin;
		GtkWidget * item;			
		char * labelStrConverted;
		
		// starting from the placeholder, find the menu item with the correct text
		found = FALSE;
		labelStrConverted = wlibConvertInput( labelStr );
		origin = i;
		
		// get menu item
		// get label of item
		// compare items
		//    if identical, leave loop
		while( i <= origin + ml->count && !found ) {
			const char * itemLabel;
			
			item = g_list_nth_data( children, i );
			itemLabel = gtk_menu_item_get_label( GTK_MENU_ITEM( item ));
			if( !g_utf8_collate (itemLabel, labelStrConverted ))
				found = TRUE;
			else
				i++;
		}
		if( found ) {
			wMenuListItem_p mold;

			mold = g_object_get_data( G_OBJECT( item ), WLISTITEM );
			// kill the menu entry
			gtk_widget_destroy( item );
			// free the data 
			free( (void *)mold->labelStr );
			free( (void *)mold );

			ml->count--;	
		}
	}	
	
	if( children )
		g_list_free( children );
}

/**
 * Get the label and the application data of a specific menu list item
 *
 * \param ml 	IN menu list 
 * \param index IN item within list
 * \param data	OUT	application data
 * \return    item label
 */

const char * 
wMenuListGet( wMenuList_p ml, int index, void ** data )
{
	int i;

	GList * children;
	const char * itemLabel = NULL;
	
	// check whether index is in range, if not return immediately
	if ( index >= ml->count || ml->count <= 0 ) {
		if (data)
			*data = NULL;
		return NULL;
	}
	
	// find the placeholder for the list in the menu
	i = getMlistOrigin( ml, &children );
	
	if( i > -1 ) {
		GtkWidget * item;
		wMenuListItem_p mold;

		item = g_list_nth_data( children, i + index + 1 );
		itemLabel = gtk_menu_item_get_label( GTK_MENU_ITEM( item ));
		mold = g_object_get_data( G_OBJECT( GTK_MENU_ITEM( item ) ), WLISTITEM );
		*data = mold->data;
	}
	
	if( children )
		g_list_free( children );
	
	return itemLabel;
}

/**
 * Remove all items from menu list
 *
 * \param ml 	IN menu item list
 */

void wMenuListClear(
	wMenuList_p ml )
{
	int origin;
	GList * children;

	if (ml->count == 0)
		return;

	origin = getMlistOrigin( ml, &children );
		
	if( origin > -1 ) {
		int i;

		i = origin;
		while( i < origin + ml->count ) {
			wMenuListItem_p mold;
			GtkWidget * item;
			
			item = g_list_nth_data( children, i + 1 );
			mold = g_object_get_data( G_OBJECT( item ), WLISTITEM );
			// kill the menu entry
			gtk_widget_destroy( item );
			// free the data 
			free( (void *)mold->labelStr );
			free( (void *)mold );
			i++;
		}	
	}	

	ml->count = 0;
	gtk_widget_show( MMENUITEM( ml ));
	
	if( children )
		g_list_free( children );
}
/*-----------------------------------------------------------------*/
/**
 * Create a check box as part of a menu
 *
 * \param m 		IN menu to be extended
 * \param helpStr 	IN reference into help
 * \param labelStr 	IN text for entry
 * \param acclKey 	IN acceleratoor key to add
 * \param set 		IN initial state
 * \param action 	IN callback function  
 * \param data 		IN application data
 * \return menu entry
 */

wMenuToggle_p wMenuToggleCreate(
	wMenu_p m, 
	const char * helpStr,
	const char * labelStr,
	long acclKey,
	wBool_t set,
	wMenuToggleCallBack_p action,
	void * data )
{
	wMenuToggle_p mt;

	mt = (wMenuToggle_p)createMenuItem( m, M_TOGGLE, helpStr, labelStr, sizeof( struct wMenuToggle_t ));
	setAcclKey( m->parent, m->menu, MMENUITEM( mt ), acclKey );
	mt->action = action;
	mt->data = data;
	mt->enabled = TRUE;
	MPARENT( mt ) = m;
	wMenuToggleSet( mt, set );
	
	return mt;
}

/**
 * Get the state of a menu check box
 *
 * \param mt 		IN menu to be extended
 * \return current state
 */

wBool_t wMenuToggleGet(
	wMenuToggle_p mt )
{
	return mt->set;
}

/**
 * Set a menu check box active / inactive
 *
 * \param mt 		IN menu to be extended
 * \param set 		IN new state
 * \return previous state
 */

wBool_t wMenuToggleSet(
	wMenuToggle_p mt,
	wBool_t set )
{
	wBool_t rc;
	if (mt==NULL) return 0;
	MRECURSION( mt )++;
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(MMENUITEM( mt )), set );
	MRECURSION( mt )--;
	rc = mt->set;
	mt->set = set;
	return rc;
}

/**
 * Enable menu entry containing a check box
 *
 * \param mi 		IN menu entry
 * \param enable 	IN new state 
 * \return
 */

void wMenuToggleEnable(
	wMenuToggle_p mt,
	wBool_t enable )
{
	mt->enabled = enable;
}


/*-----------------------------------------------------------------*/

/**
 * Set the text for a menu 
 *
 * \param m 		IN menu entry
 * \param labelStr 	IN new text
 * \return
 */

void wMenuSetLabel( wMenu_p m, const char * labelStr) {
	wlibSetLabel( m->widget, m->option, labelStr, &m->labelG, &m->imageG );
}

/**
 * Signal handler for menu items. Parameters are the GtkWidget as 
 * expected and the pointer to the MenuListItem
 *
 * \param widget IN the GtkWidget
 * \param value  IN the menu list item 
 * \return
 */

static gint pushMenu(
	GtkWidget * widget,
	wMenu_p m )
{
	gtk_menu_popup_at_pointer( GTK_MENU(m->menu), NULL);
	/* Tell calling code that we have handled this event; the buck
	 * stops here. */
	return TRUE;
}

/**
 * Create a button with a drop down menu
 *
 * \param parent 	IN parent window
 * \param x 		IN x position
 * \param y 		IN y position
 * \param helpStr 	IN help anchor string
 * \param labelStr  IN label for menu
 * \param option    IN options (Whatever they are)
 * \return pointer to the created menu
 */

wMenu_p wMenuCreate(
	wWin_p	parent,
	wPos_t	x,
	wPos_t	y,
	const char 	* helpStr,
	const char	* labelStr,
	long	option )
{
	wMenu_p m;
	m = wlibAlloc( parent, B_MENU, x, y, labelStr, sizeof( struct wMenu_t ), NULL );
	m->mmtype = MM_BUTT;
	m->option = option;
	m->traceFunc = NULL;
	m->traceData = NULL;
	wlibComputePos( (wControl_p)m );

	m->widget = gtk_button_new();
	g_signal_connect (m->widget, "clicked",
			G_CALLBACK(pushMenu), m );

	m->menu = gtk_menu_new();

	wMenuSetLabel( m, labelStr );
	
	gtk_fixed_put( GTK_FIXED(parent->widget), m->widget, m->realX, m->realY );
	wlibControlGetSize( (wControl_p)m );
	if ( m->w < 80 && (m->option&BO_ICON)==0) {
		m->w = 80;
		gtk_widget_set_size_request( m->widget, m->w, m->h );
	}
	gtk_widget_show( m->widget );
	wlibAddButton( (wControl_p)m );
	wlibAddHelpString( m->widget, helpStr );
	return m;
}

/**
 * Add a drop-down menu to the menu bar. 
 *
 * \param w 		IN main window handle 
 * \param helpStr 	IN unused (should be help topic )
 * \param labelStr 	IN label for the drop-down menu 
 * \return    pointer to the created drop-down menu
 */

wMenu_p wMenuBarAdd(
	wWin_p w,
	const char * helpStr,
	const char * labelStr )
{
	wMenu_p m;
	GtkWidget * menuItem;
	
	m = wlibAlloc( w, B_MENU, 0, 0, labelStr, sizeof( struct wMenu_t ), NULL );
	m->mmtype = MM_BAR;
	m->realX = 0;
	m->realY = 0;
	
	menuItem = gtk_menu_item_new_with_mnemonic( wlibConvertInput(m->labelStr) );
	m->menu = gtk_menu_new();
	gtk_menu_item_set_submenu( GTK_MENU_ITEM(menuItem), m->menu );
	gtk_menu_shell_append( GTK_MENU_SHELL(w->menubar), menuItem );
	gtk_widget_show( menuItem );

	m->w = 0;
	m->h = 0;
	
	/* TODO: why is help not supported here? */
	/*gtkAddHelpString( m->panel_item, helpStr );*/
	
	return m;
}


/*-----------------------------------------------------------------*/

/**
 * Create a popup menu (context menu)
 *
 * \param w 		IN parent window
 * \param labelStr 	IN label 
 * \return    the created menu
 */

wMenu_p wMenuPopupCreate(
	wWin_p w,
	const char * labelStr )
{
	wMenu_p b;
	b = wlibAlloc( w, B_MENU, 0, 0, labelStr, sizeof *b, NULL );
	b->mmtype = MM_POPUP;
	b->option = 0;

	b->menu = gtk_menu_new();
	b->w = 0;
	b->h = 0;
	g_signal_connect( b->menu, "key_press_event",
			G_CALLBACK(catch_shift_ctrl_alt_keys), b);
	g_signal_connect( b->menu, "key_release_event",
			G_CALLBACK (catch_shift_ctrl_alt_keys), b);
	gtk_widget_set_events ( GTK_WIDGET(b->menu), GDK_EXPOSURE_MASK|GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK );
	return b;
}

/**
 * Show a context menu
 *
 * \param mp IN the context menu
 */

void wMenuPopupShow( wMenu_p mp )
{
	gtk_menu_popup_at_pointer( GTK_MENU(mp->menu), NULL );
}


/*-----------------------------------------------------------------*/

/**
 * ?? Seems to be related to macro / automatic playback functionality
 *
 * \param m 	IN 
 * \param func 	IN 
 * \param data 	IN 
 */

void wMenuSetTraceCallBack(
	wMenu_p m,
	wMenuTraceCallBack_p func,
	void * data )
{
	m->traceFunc = func;
	m->traceData = data;
}

/**
 * ??? same as above 
 * \param m 	IN 
 * \param label IN 
 * \return    describe the return value
 */

wBool_t wMenuAction(
	wMenu_p m,
	const char * label )
{
	wMenuItem_p mi;
	wMenuToggle_p mt;
	for ( mi = m->first; mi != NULL; mi = (wMenuItem_p)mi->next ) {
		if ( strcmp( mi->labelStr, label ) == 0 ) {
			switch( MITEMTYPE( mi )) {
			case M_SEPARATOR:
				break;
			case M_PUSH:
				if ( ((wMenuPush_p)mi)->enabled == FALSE )
					wBeep();
				else
					((wMenuPush_p)mi)->action( ((wMenuPush_p)mi)->data );
				break;
			case M_TOGGLE:
				mt = (wMenuToggle_p)mi;
				if ( mt->enabled == FALSE ) {
					wBeep();
				} else {
					wMenuToggleSet( mt, !mt->set );
					mt->action( mt->set, mt->data );
				}
				break;
			case M_MENU:
				break;
			case M_LIST:
				break;
			default:
				/*fprintf(stderr, "Oops: wMenuAction\n");*/
			break;
			}
			return TRUE;
		}
	}
	return FALSE;
}
