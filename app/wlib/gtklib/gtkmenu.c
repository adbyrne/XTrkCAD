/** \file gtkmenu.c
 * Menu creation and handling stuff.
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkmenu.c,v 1.5 2009-10-03 04:49:01 dspagnol Exp $
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
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#include "gtkint.h"

int testMenuPopup = 1;

static char MenuItemDataKey[] = "MenuItemDataKey";

extern char gtkAccelChar;

/*
 *****************************************************************************
 *
 * Menus
 *
 *****************************************************************************
 */

typedef enum { M_MENU, M_SEPARATOR, M_PUSH, M_LIST, M_LISTITEM, M_TOGGLE, M_RADIO } mtype_e;
typedef enum { MM_BUTT, MM_MENU, MM_BAR, MM_POPUP } mmtype_e;


#define MOBJ_COMMON \
	WOBJ_COMMON \
	mtype_e mtype; \
	GtkWidget * menu_item; \
	wMenu_p parentMenu; \
	int recursion;
	
struct wMenuItem_t {
	MOBJ_COMMON
	};
typedef struct wMenuItem_t * wMenuItem_p;

struct wMenu_t {
	MOBJ_COMMON
	mmtype_e mmtype;
	wMenuItem_p first, last;
	GSList *radioGroup;			/* in case menu holds a radio button group */
	GtkWidget * menu;
	wMenuTraceCallBack_p traceFunc;
	void * traceData;
	GtkLabel * labelG;
	GtkWidget * imageG;
	};

struct wMenuPush_t {
	MOBJ_COMMON
	wMenuCallBack_p action;
	wBool_t enabled;
	};

struct wMenuRadio_t {
	MOBJ_COMMON
	wMenuCallBack_p action;
	wBool_t enabled;
	};


typedef struct wMenuListItem_t * wMenuListItem_p;

struct wMenuList_t {
	MOBJ_COMMON
	int max;
	int count;
	wMenuListCallBack_p action;
	};

struct wMenuListItem_t {
	MOBJ_COMMON
	wMenuList_p mlist;
	};

struct wMenuToggle_t {
	MOBJ_COMMON
	wMenuToggleCallBack_p action;
	wBool_t enabled;
	wBool_t set;
	};


/*-----------------------------------------------------------------*/

static void pushMenuItem(
	GtkWidget * widget,
	gpointer value )
{
	wMenuItem_p m = (wMenuItem_p)value;
	wMenuToggle_p mt;
	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(0);
	}
/*	wFlush(); */
	if (m->recursion)
		return;
	switch (m->mtype) {
	case M_PUSH:
		if ( ((wMenuPush_p)m)->enabled == FALSE )
			wBeep();
		else
			((wMenuPush_p)m)->action( ((wMenuPush_p)m)->data );
		break;
	case M_TOGGLE:
		mt = (wMenuToggle_p)m;
		if ( mt->enabled == FALSE ) {
			wBeep();
		} else {
			wMenuToggleSet( mt, !mt->set );
			mt->action( mt->set, mt->data );
		}
		break;
	case M_RADIO:
		/* NOTE: action is only called when radio button is activated, not when deactivated */
		if ( ((wMenuRadio_p)m)->enabled == FALSE )
			wBeep();
		else
			if( ((GtkCheckMenuItem *)widget)->active == TRUE )
				((wMenuRadio_p)m)->action( ((wMenuRadio_p)m)->data );
		break;	
	case M_MENU:
		return;
	default:
		/*fprintf(stderr," Oops menu\n");*/
		return;
	}
	if ( (m->parentMenu)->traceFunc ) {
		(m->parentMenu)->traceFunc( m->parentMenu, m->labelStr,  ((wMenu_p)m->parentMenu)->traceData );
	}
} 

static wMenuItem_p createMenuItem(
	wMenu_p m,
	mtype_e mtype,
	const char * helpStr,
	const char * labelStr,
	int size )
{
	wMenuItem_p mi;
	mi = (wMenuItem_p)gtkAlloc( NULL, B_MENUITEM, 0, 0, labelStr, size, NULL );
	mi->mtype = mtype;
	switch ( mtype ) {
	case M_LIST:
		m->menu_item = NULL;
		break;
	case M_SEPARATOR:
		mi->menu_item = gtk_separator_menu_item_new();
		break;
	case M_TOGGLE:
		mi->menu_item = gtk_check_menu_item_new_with_mnemonic(gtkConvertInput(mi->labelStr));
		break;
	case M_RADIO:
		mi->menu_item = gtk_radio_menu_item_new_with_mnemonic(m->radioGroup, gtkConvertInput(mi->labelStr));
		m->radioGroup = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (mi->menu_item));
		break;			
	default:
		mi->menu_item = gtk_menu_item_new_with_mnemonic(gtkConvertInput(mi->labelStr));
		break;
	}
	if (mi->menu_item) {
		if (m)
			gtk_menu_append( GTK_MENU(m->menu), mi->menu_item );

		gtk_signal_connect( GTK_OBJECT(mi->menu_item), "activate",
				GTK_SIGNAL_FUNC(pushMenuItem), mi );
 		gtk_widget_show(mi->menu_item);
	}
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
		gtkAddHelpString( mi->menu_item, helpStr );
	}
	mi->parentMenu = m;
	return mi;
}


static void setAcclKey( wWin_p w, GtkWidget * menu, GtkWidget * menu_item, int acclKey )
{
	char acclStr[40];
	int len;
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
#ifdef LATER
 		switch ( (acclKey&0xFF) ) {
		case '+': acclKey = (acclKey&0xFF00) | WSHIFT | '='; break;
		case '?': acclKey = (acclKey&0xFF00) | WSHIFT | '/'; break;
		}
#endif
		len = 0;
		if (acclKey&WALT) {
			mask |= GDK_MOD1_MASK;
			strcpy( acclStr+len, "Meta+" );
			len += 5;
		}
		if (acclKey&WSHIFT) {
			mask |= GDK_SHIFT_MASK;
			strcpy( acclStr+len, "Shift+" );
			len += 6;
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
			strcpy( acclStr+len, "Ctrl+" );
			len += 5;
		}
		acclStr[len++] = (acclKey & 0xFF);
		acclStr[len++] = '\0';
		gtk_widget_add_accelerator( menu_item, "activate",
			(isalpha(acclKey&0xFF)?accel_alpha_group:accel_nonalpha_group),
			toupper(acclKey&0xFF), mask, GTK_ACCEL_VISIBLE|GTK_ACCEL_LOCKED );
	}
}

/*-----------------------------------------------------------------*/

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
	if (m->mmtype == MM_POPUP && !testMenuPopup)
		return mi;
	setAcclKey( m->parent, m->menu, mi->menu_item, acclKey );
	mi->action = action;
	mi->data = data;
	mi->enabled = TRUE;
	return mi;
}

void wMenuRadioSetActive( 
	wMenuRadio_p mi )
{
	gtk_check_menu_item_set_active( (GtkCheckMenuItem *)mi->menu_item, TRUE ); 
}	 		

/*-----------------------------------------------------------------*/

wMenuPush_p wMenuPushCreate(
	wMenu_p m, 
	const char * helpStr,
	const char * labelStr,
	long acclKey,
	wMenuCallBack_p action,
	void 	*data )
{
	wMenuPush_p mi;

	mi = (wMenuPush_p)createMenuItem( m, M_PUSH, helpStr, labelStr, sizeof *mi );
	if (m->mmtype == MM_POPUP && !testMenuPopup)
		return mi;
	setAcclKey( m->parent, m->menu, mi->menu_item, acclKey );
	mi->action = action;
	mi->data = data;
	mi->enabled = TRUE;
	return mi;
}


void wMenuPushEnable(
	wMenuPush_p mi,
	wBool_t enable )
{
	mi->enabled = enable;
	gtk_widget_set_sensitive( GTK_WIDGET(mi->menu_item), enable );
}


/*-----------------------------------------------------------------*/

wMenu_p wMenuMenuCreate(
	wMenu_p m, 
	const char * helpStr,
	const char * labelStr )
{
	wMenu_p mi;
	mi = (wMenu_p)createMenuItem( m, M_MENU, helpStr, labelStr, sizeof *mi );
	mi->mmtype = MM_MENU;
	mi->menu = gtk_menu_new();
	/*gtk_widget_set_sensitive( GTK_WIDGET(mi->menu_item), FALSE );*/
	gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi->menu_item), mi->menu );
	return mi;
}


/*-----------------------------------------------------------------*/

void wMenuSeparatorCreate(
	wMenu_p m ) 
{
	wMenuItem_p mi;
	mi = createMenuItem( m, M_SEPARATOR, NULL, "", sizeof *mi );
}


/*-----------------------------------------------------------------*/

int getMlistOrigin( wMenu_p m, wMenuList_p ml )
{
	wMenuItem_p mi;
	int count;
	count = 0;	/* Menu counts as one */
	for ( mi = m->first; mi != NULL; mi = (wMenuItem_p)mi->next ) {
		switch( mi->mtype ) {
		case M_SEPARATOR:
		case M_PUSH:
		case M_MENU:
			count++;
			break;
		case M_LIST:
			if (mi == (wMenuItem_p)ml)
			return count;
			count += ((wMenuList_p)mi)->count;
			break;
		default:
			/*fprintf(stderr, "Oops: getMlistOrigin\n");*/
			break;
		}
	}
	return count;
}

wMenuList_p wMenuListCreate(
	wMenu_p m, 
	const char * helpStr,
	int max,
	wMenuListCallBack_p action )
{
	wMenuList_p mi;
	mi = (wMenuList_p)createMenuItem( m, M_LIST, NULL, NULL, sizeof *mi );
	mi->next = NULL;
	mi->count = 0;
	mi->max = max;
	mi->parentMenu = m;
	mi->action = action;
	return (wMenuList_p)mi;
}


static void pushMenuList(
	GtkWidget * widget,
	gpointer value )
{
	wMenuListItem_p ml = (wMenuListItem_p)value;
	int i;
	int origin;
	GtkWidget * item;
	char * itemLabel;
	GList * children;
	GList * child;
	GtkWidget *label;

	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(0);
	}
	wFlush();

	if (ml->recursion)
		return;
	if (ml->mlist->count <= 0) {
		fprintf( stderr, "pushMenuItem: empty list\n" );
		return;
	}
	if (ml->mlist->action) {
		origin = getMlistOrigin(ml->mlist->parentMenu, ml->mlist);
		children = gtk_container_children( GTK_CONTAINER(ml->mlist->parentMenu->menu) );
		if (children == NULL) abort();
		child = g_list_nth( children, origin );
		for (i=origin; i<origin+ml->mlist->count; i++, child=g_list_next(child) ) {
			if (child == NULL) abort();
			item = (GtkWidget*)child->data;
			if (item == NULL) abort();
			if (item == widget) {
				children = gtk_container_children(GTK_CONTAINER(item));
				label = (GtkWidget*)children->data;
				gtk_label_get( GTK_LABEL(label), &itemLabel );
				ml->mlist->action( i-origin, itemLabel, ml->data );
				return;
			}
		}
	}
	fprintf( stderr, "pushMenuItem: item (%lx) not found\n", (long)widget );
}


void wMenuListAdd(
	wMenuList_p ml,
	int index,
	const char * labelStr,
	const void * data )
{
	int i;
	int origin;
	GtkWidget * item;
	char * itemLabel;
	GList * children;
	GList * child;
	GList * itemList;
	GtkWidget * label;
	wMenuListItem_p mi;
	char * labelStrConverted;

	origin = getMlistOrigin(ml->parentMenu, ml);
	if (ml->count > 0) {
		children = gtk_container_children( GTK_CONTAINER(ml->parentMenu->menu) );
		if (children == NULL) abort();
		child = g_list_nth( children, origin );
		labelStrConverted = gtkConvertInput(labelStr);
		for (i=origin; i<origin+ml->count; i++, child=g_list_next(child) ) {
			if (child == NULL) abort();
			item = (GtkWidget*)child->data;
			if (item == NULL) abort();
			itemList = gtk_container_children(GTK_CONTAINER(item));
			label = (GtkWidget*)itemList->data;
			gtk_label_get( GTK_LABEL(label), &itemLabel );
			if (strcmp( labelStrConverted, itemLabel ) == 0) {
				if (i != ml->count+index) {
					gtk_container_remove( GTK_CONTAINER(ml->parentMenu->menu), item );
					ml->count--;
					break;
				}
				return;
			}
		}
		if (ml->max >= 0 && ml->count >= ml->max) {
			child = g_list_nth( children, origin+ml->count-1 );
			if (child == NULL) abort();
			item = (GtkWidget*)child->data;
			if (item == NULL) abort();
			gtk_container_remove( GTK_CONTAINER(ml->parentMenu->menu), item );
			ml->count--;
		}
	}
	mi = (wMenuListItem_p)gtkAlloc( NULL, B_MENUITEM, 0, 0, labelStr, sizeof *mi, NULL );
	mi->mtype = M_LISTITEM;
	mi->menu_item = gtk_menu_item_new_with_label(gtkConvertInput(mi->labelStr));
	mi->data = (void *)data;
	mi->mlist = ml;
	if (index < 0 || index > ml->count)
	index = ml->count;
	gtk_menu_insert( GTK_MENU(ml->parentMenu->menu), mi->menu_item, origin+index );
	gtk_signal_connect( GTK_OBJECT(mi->menu_item), "activate",
				GTK_SIGNAL_FUNC(pushMenuList), mi );
	gtk_object_set_data( GTK_OBJECT(mi->menu_item), MenuItemDataKey, mi );
	gtk_widget_show(mi->menu_item);

	ml->count++;
}


void wMenuListDelete(
	wMenuList_p ml,
	const char * labelStr )
{
	int i;
	int origin;
	GtkWidget * item;
	char * itemLabel;
	GList * children;
	GList * child;
	GtkWidget * label;
	char * labelStrConverted;

	if (ml->count < 0) abort();
	origin = getMlistOrigin(ml->parentMenu, ml);
	children = gtk_container_children( GTK_CONTAINER(ml->parentMenu->menu) );
	if (children == NULL) abort();
	child = g_list_nth( children, origin );
	labelStrConverted = gtkConvertInput( labelStr );
	for (i=origin; i<origin+ml->count; i++, child=g_list_next(child) ) {
		if (child == NULL) abort();
		item = (GtkWidget*)child->data;
		if (item == NULL) abort();
		children = gtk_container_children(GTK_CONTAINER(item));
		label = (GtkWidget*)children->data;
		gtk_label_get( GTK_LABEL(label), &itemLabel );
		if (strcmp( labelStrConverted, itemLabel ) == 0) {
			gtk_container_remove( GTK_CONTAINER(ml->parentMenu->menu), item );
			gtk_widget_queue_resize( GTK_WIDGET(ml->parentMenu->menu) );
			ml->count--;
			return;
		}
	}
}


const char * wMenuListGet( wMenuList_p ml, int index, void ** data )
{
	int origin;
	GtkWidget * item;
	GList * children;
	GList * child;
	GtkWidget * label;
	char * itemLabel;
	wMenuListItem_p mi;

	if (ml->count <= 0)
	return NULL;

	if (index >= ml->count) {
		if (data)
			*data = NULL;
		return NULL;
	}
	origin = getMlistOrigin(ml->parentMenu, ml);
	children = gtk_container_children( GTK_CONTAINER(ml->parentMenu->menu) );
	if (children == NULL) abort();
	child = g_list_nth( children, origin+index );
	if (child == NULL) abort();
	item = (GtkWidget*)child->data;
	if (item == NULL) abort();
	children = gtk_container_children(GTK_CONTAINER(item));
	label = (GtkWidget*)children->data;
	gtk_label_get( GTK_LABEL(label), &itemLabel );
	if (data) {
		mi = (wMenuListItem_p)gtk_object_get_data( GTK_OBJECT(item), MenuItemDataKey );
		if (mi)
			*data = mi->data;
	}
	return itemLabel;
}


void wMenuListClear(
	wMenuList_p ml )
{
	int i;
	int origin;
	GtkWidget * item;
	GList * children;
	GList * child;

	if (ml->count == 0)
		return;
	origin = getMlistOrigin(ml->parentMenu, ml);
	children = gtk_container_children( GTK_CONTAINER(ml->parentMenu->menu) );
	if (children == NULL) abort();
	child = g_list_nth( children, origin );
	for (i=origin; i<origin+ml->count; i++, child=g_list_next(child) ) {
		if (child == NULL) abort();
		item = (GtkWidget*)child->data;
		if (item == NULL) abort();
		gtk_container_remove( GTK_CONTAINER(ml->parentMenu->menu), item );
	}
	ml->count = 0;
	gtk_widget_queue_resize( GTK_WIDGET(ml->parentMenu->menu) );
}
/*-----------------------------------------------------------------*/

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

	mt = (wMenuToggle_p)createMenuItem( m, M_TOGGLE, helpStr, labelStr, sizeof *mt );
	setAcclKey( m->parent, m->menu, mt->menu_item, acclKey );
	mt->action = action;
	mt->data = data;
	mt->enabled = TRUE;
	mt->parentMenu = m;
	wMenuToggleSet( mt, set );
	
	return mt;
}


wBool_t wMenuToggleGet(
	wMenuToggle_p mt )
{
	return mt->set;
}


wBool_t wMenuToggleSet(
	wMenuToggle_p mt,
	wBool_t set )
{
	wBool_t rc;
	if (mt==NULL) return 0;
	mt->recursion++;
	gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM(mt->menu_item), set );
	mt->recursion--;
	rc = mt->set;
	mt->set = set;
	return rc;
}


void wMenuToggleEnable(
	wMenuToggle_p mt,
	wBool_t enable )
{
	mt->enabled = enable;
}


/*-----------------------------------------------------------------*/

void wMenuSetLabel( wMenu_p m, const char * labelStr) {
	gtkSetLabel( m->widget, m->option, labelStr, &m->labelG, &m->imageG );
}


static gint pushMenu(
	GtkWidget * widget,
	wMenu_p m )
{
	gtk_menu_popup( GTK_MENU(m->menu), NULL, NULL, NULL, NULL, 0, 0 );
	/* Tell calling code that we have handled this event; the buck
	 * stops here. */
	return TRUE;
}


wMenu_p wMenuCreate(
	wWin_p	parent,
	wPos_t	x,
	wPos_t	y,
	const char 	* helpStr,
	const char	* labelStr,
	long	option )
{
	wMenu_p m;
	m = gtkAlloc( parent, B_MENU, x, y, labelStr, sizeof *m, NULL );
	m->mmtype = MM_BUTT;
	m->option = option;
	m->traceFunc = NULL;
	m->traceData = NULL;
	gtkComputePos( (wControl_p)m );

	m->widget = gtk_button_new();
	gtk_signal_connect (GTK_OBJECT(m->widget), "clicked",
			GTK_SIGNAL_FUNC(pushMenu), m );

	m->menu = gtk_menu_new();

	wMenuSetLabel( m, labelStr );
#ifdef MENUOPTION
	gtk_option_menu_set_menu( GTK_OPTION_MENU(m->widget), m->menu );
	((GtkOptionMenu*)m->widget)->width = 25;
	((GtkOptionMenu*)m->widget)->height = 16;
#endif
	
	gtk_fixed_put( GTK_FIXED(parent->widget), m->widget, m->realX, m->realY );
	gtkControlGetSize( (wControl_p)m );
	if ( m->w < 80 && (m->option&BO_ICON)==0) {
		m->w = 80;
		gtk_widget_set_usize( m->widget, m->w, m->h );
	}
	gtk_widget_show( m->widget );
	gtkAddButton( (wControl_p)m );
	gtkAddHelpString( m->widget, helpStr );
	return m;
}

/**
 * Add a drop-down menu to the menu bar. 
 *
 * \param[IN] w main window handle 
 * \param[IN] helpStr unused (should be help topic )
 * \param[IN] labelStr label for the drop-down menu 
 * \return    pointer to the created drop-down menu
 */


wMenu_p wMenuBarAdd(
	wWin_p w,
	const char * helpStr,
	const char * labelStr )
{
	wMenu_p m;
	GtkWidget * menuItem;
	static GtkAccelGroup * accel_group = NULL;
	
	m = gtkAlloc( w, B_MENU, 0, 0, labelStr, sizeof *m, NULL );
	m->mmtype = MM_BAR;
	m->realX = 0;
	m->realY = 0;
	
	menuItem = gtk_menu_item_new_with_label( gtkConvertInput(m->labelStr) );
	m->menu = gtk_menu_new();
	gtk_menu_item_set_submenu( GTK_MENU_ITEM(menuItem), m->menu );
	gtk_menu_bar_append( GTK_MENU_BAR(w->menubar), menuItem );
	gtk_widget_show( menuItem );

	m->w = 0;
	m->h = 0;
	
	/* TODO: why is help not supported here? */
	/*gtkAddHelpString( m->panel_item, helpStr );*/
	
	if ( gtkAccelChar ) {
		if ( accel_group == NULL ) {
			accel_group = gtk_accel_group_new();
			gtk_window_add_accel_group( GTK_WINDOW(w->gtkwin), accel_group );
		}
		gtk_widget_add_accelerator( menuItem, "activate", accel_group, tolower(gtkAccelChar), GDK_MOD1_MASK, GTK_ACCEL_LOCKED );
	}
	return m;
}


/*-----------------------------------------------------------------*/


wMenu_p wMenuPopupCreate(
	wWin_p w,
	const char * labelStr )
{
	wMenu_p b;
	b = gtkAlloc( w, B_MENU, 0, 0, labelStr, sizeof *b, NULL );
	b->mmtype = MM_POPUP;
	b->option = 0;

	b->menu = gtk_menu_new();
	b->w = 0;
	b->h = 0;
	gtk_signal_connect( GTK_OBJECT (b->menu), "key_press_event",
			GTK_SIGNAL_FUNC (catch_shift_ctrl_alt_keys), b);
	gtk_signal_connect( GTK_OBJECT (b->menu), "key_release_event",
			GTK_SIGNAL_FUNC (catch_shift_ctrl_alt_keys), b);
	gtk_widget_set_events ( GTK_WIDGET(b->menu), GDK_EXPOSURE_MASK|GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK );
	return b;
}


void wMenuPopupShow( wMenu_p mp )
{
	gtk_menu_popup( GTK_MENU(mp->menu), NULL, NULL, NULL, NULL, 0, 0 );
}


/*-----------------------------------------------------------------*/

void wMenuSetTraceCallBack(
	wMenu_p m,
	wMenuTraceCallBack_p func,
	void * data )
{
	m->traceFunc = func;
	m->traceData = data;
}

wBool_t wMenuAction(
	wMenu_p m,
	const char * label )
{
	wMenuItem_p mi;
	wMenuToggle_p mt;
	for ( mi = m->first; mi != NULL; mi = (wMenuItem_p)mi->next ) {
		if ( strcmp( mi->labelStr, label ) == 0 ) {
			switch( mi->mtype ) {
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
