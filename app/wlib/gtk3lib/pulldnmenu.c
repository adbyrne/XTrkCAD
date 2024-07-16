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

/*
 *****************************************************************************
 *
 * Menus
 *
 *****************************************************************************
 */

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
    wControl_p m = (wControl_p)value;
    struct menuitem *mi = WLIB_GET_DATA_PTR(m, menuitem);

    switch( m->type ){
    case M_PUSH:
    case M_TOGGLE:
        mi->action( m->context );
        break;
    //case M_TOGGLE:
    //    mt = (wMenuToggle_p)m;
    //    mt->action( mt->data );
    //    break;
    case M_RADIO:
        /* NOTE: action is only called when radio button is activated,
        not when deactivated */
        if( gtk_check_menu_item_get_active((GtkCheckMenuItem *)widget ) == TRUE )
            mi->action(m->context);
        break;
    case M_MENU:
        return;
    default:
        /*fprintf(stderr," Oops menu\n");*/
        return;
    }
    // if( MPARENT(m)->traceFunc ) {
    // 	MPARENT(m)->traceFunc( MPARENT( m ), m->oc.labelStr,  MPARENT(m)->traceData );
    //}
}

/**
 * Add a accelerator key to a widget
 *
 * @param w         IN unused(?)
 * @param menu      IN unused(?)
 * @param menu_item IN owning widget
 * @param acclKey   IN the accelerator key
 */

static void setAcclKey( GtkWidget* menu_item, int acclKey)
{
    int mask = 0;
    GtkAccelGroup* accel_group = wlibAppWinGetAccelGroup();

    if (acclKey & WALT) {
        mask |= GDK_MOD1_MASK;
    }
    if (acclKey & WSHIFT) {
        mask |= GDK_SHIFT_MASK;
        switch ((acclKey & 0xFF)) {
        case '0':
            acclKey += ')' - '0';
            break;
        case '1':
            acclKey += '!' - '1';
            break;
        case '2':
            acclKey += '@' - '2';
            break;
        case '3':
            acclKey += '#' - '3';
            break;
        case '4':
            acclKey += '$' - '4';
            break;
        case '5':
            acclKey += '%' - '5';
            break;
        case '6':
            acclKey += '^' - '6';
            break;
        case '7':
            acclKey += '&' - '7';
            break;
        case '8':
            acclKey += '*' - '8';
            break;
        case '9':
            acclKey += '(' - '9';
            break;
        case '`':
            acclKey += '~' - '`';
            break;
        case '-':
            acclKey += '_' - '-';
            break;
        case '=':
            acclKey += '+' - '=';
            break;
        case '\\':
            acclKey += '|' - '\\';
            break;
        case '[':
            acclKey += '{' - '[';
            break;
        case ']':
            acclKey += '}' - ']';
            break;
        case ';':
            acclKey += ':' - ';';
            break;
        case '\'':
            acclKey += '"' - '\'';
            break;
        case ',':
            acclKey += '<' - ',';
            break;
        case '.':
            acclKey += '>' - '.';
            break;
        case '/':
            acclKey += '?' - '/';
            break;
        default:
            break;
        }
    }
    if (acclKey & WCTL) {
        mask |= GDK_CONTROL_MASK;
    }

    gtk_widget_add_accelerator(menu_item,
                               "activate",
                               accel_group,
                               acclKey & 0xFF, mask, GTK_ACCEL_VISIBLE);
}

/**
 * Create a new menu element, add to the parent menu and to help
 *
 * \param m 		IN parent menu
 * \param mi		IN menu item properties
 * \param mtype 	IN type of new entry
 * \param helpStr 	IN help topic
 * \param labelStr 	IN display label
 */

static void CreateMenuItem(
    wControl_p m,
    wControl_p mi,
    wType_e mtype,
    const char * helpStr,
    const char * labelStr,
    int acclKey )
{
//    MITEMTYPE( mi )= mtype;

    switch ( mtype ) {
    case M_SEPARATOR:
        mi->widget = gtk_separator_menu_item_new();
        break;
    case M_TOGGLE:
        mi->widget = gtk_check_menu_item_new_with_mnemonic(
                              wlibConvertInput(labelStr));
        g_signal_connect(mi->widget, "toggled", G_CALLBACK(pushMenuItem),
                         mi);
        break;
    case M_RADIO:
        mi->widget = gtk_radio_menu_item_new_with_mnemonic(m->data.menu.radioGroup,
                          wlibConvertInput(labelStr));
        m->data.menu.radioGroup = gtk_radio_menu_item_get_group (
                            GTK_RADIO_MENU_ITEM ( mi->widget ));
        g_signal_connect(mi->widget, "activate", G_CALLBACK(pushMenuItem),
                         mi);
        break;
    case M_PUSH:
        mi->widget = gtk_menu_item_new_with_mnemonic(
                              wlibConvertInput(labelStr));
        g_signal_connect(mi->widget, "activate",G_CALLBACK(pushMenuItem),
                         mi);
        break;

    default:
        g_abort();
        break;
    }

    if (mi->widget) {
        if (acclKey) {
            setAcclKey(mi->widget, acclKey);
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(m->widget), mi->widget);
        gtk_widget_show(GTK_WIDGET(mi->widget));
    }

    if (helpStr != NULL) {
//		wlibAddHelpString( MMENUITEM( mi ), helpStr );
    }

    return;
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

wControl_p wMenuRadioCreate(
    wControl_p m,
    const char * helpStr,
    const char * labelStr,
    long acclKey,
    wMenuCallBack_p action,
    void 	*data )
{
    struct menuitem* menuitem;

    wControl_p mi = wlibControlNew(M_RADIO, m, helpStr, data);
    mi->context = data;

    menuitem = WLIB_GET_DATA_PTR(mi, menuitem);
    menuitem->action = action;

    CreateMenuItem( m, mi, M_RADIO, helpStr, labelStr, acclKey );

    return mi;
}

/**
 * Set radio button active
 *
 * \param mi 		IN menu entry for radio button
 * \return
 */

void wMenuRadioSetActive(
    wControl_p mi )
{
    gtk_check_menu_item_set_active( (GtkCheckMenuItem *)mi->widget, TRUE );
}

/*
 * push buttons in menu
 */



/**
 * Create a menu entry
 *
 * \param m 		IN menu to be extended
 * \param helpStr 	IN reference into help
 * \param labelStr 	IN text for entry
 * \param acclKey 	IN accelerator key to add
 * \param action 	IN callback function
 * \param data 		IN application data
 * \return menu entry
 */

wControl_p wMenuPushCreate(
    wControl_p m,
    const char * helpStr,
    const char * labelStr,
    long acclKey,
    wMenuCallBack_p action,
    void 	*data )
{
    struct menuitem* menuitem;

    wControl_p mi = wlibControlNew(M_PUSH, m, helpStr, data);
    menuitem = WLIB_GET_DATA_PTR(mi, menuitem);
    menuitem->action = action;

    CreateMenuItem( m, mi, M_PUSH, helpStr, labelStr, acclKey);
    mi->context = data;

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
    wControl_p mi,
    wBool_t enable )
{
    gtk_widget_set_sensitive( GTK_WIDGET( mi->widget ), enable );
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

wControl_p wMenuMenuCreate(
    wControl_p m,
    const char * helpStr,
    const char * labelStr )
{
    wControl_p mm;
    struct menu *menu;
    GtkWidget* submenu;
    GtkWidget* menuitem;

    mm = wlibControlNew(M_SUBMENU, m, helpStr, NULL);
    menu = WLIB_GET_DATA_PTR(mm, menu);
    menu->radioGroup = NULL;

    menuitem = gtk_menu_item_new_with_mnemonic(wlibConvertInput(labelStr));

    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( menuitem ), submenu );

    gtk_menu_shell_append(GTK_MENU_SHELL(m->widget), menuitem);
    gtk_widget_show(GTK_WIDGET(menuitem));

    mm->widget = submenu;
    return mm;
}


/*-----------------------------------------------------------------*/
/**
 * Create a menu separator
 *
 * \param mi 		IN menu entry
 * \return
 */

void wMenuSeparatorCreate(
    wControl_p m )
{
    wControl_p mi = wlibControlNew(M_SEPARATOR, m, NULL, NULL );

    CreateMenuItem( m, mi, M_SEPARATOR, NULL, "", 0);
}


/*
 * Toggle buttons in menu
 */
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

wControl_p wMenuToggleCreate(
    wControl_p m,
    const char * helpStr,
    const char * labelStr,
    long acclKey,
    wBool_t set,
    wMenuCallBack_p action,
    void * data )
{
    struct menuitem *menuitem;

    wControl_p mt = wlibControlNew(M_TOGGLE, m, helpStr, data);
    menuitem = WLIB_GET_DATA_PTR(mt, menuitem);
    menuitem->action = action;

    CreateMenuItem(m, mt, M_TOGGLE, helpStr, labelStr, acclKey );

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
    wControl_p mt )
{
    return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM( mt->widget ));
}

/**
 * Set a menu check box active / inactive
 *
 * \param mt 		IN menu to be extended
 * \param set 		IN new state
 * \return previous state
 */

wBool_t wMenuToggleSet(
    wControl_p mt,
    wBool_t set )
{
    wBool_t oldState;
    if (mt==NULL) return 0;

    oldState = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM( mt->widget ));
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(mt->widget ), set );

    return oldState;
}

/**
 * Enable menu entry containing a check box
 *
 * \param mi 		IN menu entry
 * \param enable 	IN new state
 * \return
 */

void wMenuToggleEnable(
    wControl_p mt,
    wBool_t enable )
{
    gtk_widget_set_sensitive ( GTK_WIDGET( mt->widget ), enable );
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
    printf("%s:%d Not implemented!", __FILE__, __LINE__);
//	 wlibSetLabel( m->oc.widget, m->option, labelStr, &m->labelG, &m->imageG );
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
    wWinPix_t	x,
    wWinPix_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long	option )
{
    wMenu_p m = NULL;
    // m = wlibAlloc( parent, B_MENU, x, y, labelStr, sizeof( struct wMenu_t ), NULL );
    // m->mmtype = MM_BUTT;
    // m->option = option;
    // m->traceFunc = NULL;
    // m->traceData = NULL;
    // wlibComputePos( (wControl_p)m );

    // m->widget = gtk_button_new();

    // g_signal_connect (m->widget, "clicked",
    // 		G_CALLBACK(pushMenu), m );

    // m->menu = gtk_menu_new();

    // 	wMenuSetLabel( m, labelStr );
    // 	gtk_fixed_put( GTK_FIXED(parent->widget), m->widget, m->realX, m->realY );
    // 	wlibControlGetSize( (wControl_p)m );
    // 	if ( m->w < 80 && (m->option&BO_ICON)==0) {
    // 		m->w = 80;
    // 		gtk_widget_set_size_request( m->widget, m->w, m->h );
    // 	}
    // gtk_widget_show( m->widget );
    // wlibAddButton( (wControl_p)m );
    // wlibAddHelpString( m->widget, helpStr );
    printf("%s:%d Not implemented!", __FILE__, __LINE__);
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

wControl_p wMenuBarAdd(
    wControl_p w,
    const char * helpStr,
    const char * labelStr )
{
    wControl_p m;
    struct menu* menu;
    GtkWidget *menuItem;

    m = wlibControlNew(M_SUBMENU, NULL, helpStr, NULL);
    menu = WLIB_GET_DATA_PTR(m, menu);
    menu->radioGroup = NULL;

    menuItem = gtk_menu_item_new_with_mnemonic(labelStr);
    gtk_menu_shell_append(GTK_MENU_SHELL(w->data.window.menubar), menuItem);

    m->widget = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuItem), m->widget );

    gtk_widget_show(menuItem);

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
    wMenu_p b = NULL;
    // b = wlibAlloc( w, B_MENU, 0, 0, labelStr, sizeof *b, NULL );
    // b->mmtype = MM_POPUP;
    // b->option = 0;

    // b->menu = gtk_menu_new();
    // b->w = 0;
    // b->h = 0;
    // g_signal_connect( G_OBJECT (b->menu), "key_press_event",
    // 		G_CALLBACK(catch_shift_ctrl_alt_keys), b);
    // g_signal_connect( G_OBJECT (b->menu), "key_release_event",
    // 		G_CALLBACK (catch_shift_ctrl_alt_keys), b);
    // gtk_widget_set_events ( GTK_WIDGET(b->menu), GDK_EXPOSURE_MASK|GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK );
    printf("%s:%d Not implemented!", __FILE__, __LINE__);
    return b;
}

/**
 * Show a context menu
 *
 * \param mp IN the context menu
 */

//void wMenuPopupShow( wMenu_p mp )
//{
//    gtk_menu_popup_at_pointer( GTK_MENU(mp->menu), NULL );
//}


/*-----------------------------------------------------------------*/

/**
 * ?? Seems to be related to macro / automatic playback functionality
 *
 * \param m 	IN
 * \param func 	IN
 * \param data 	IN
 */

void wMenuSetTraceCallBack(
    wControl_p m,
    wMenuTraceCallBack_p func,
    void * data )
{
    struct menu* menu = WLIB_GET_DATA_PTR(m, menu);
    menu->traceFunc = func;
    menu->traceData = data;
}

/**
 * ??? same as above
 * \param m 	IN
 * \param label IN
 * \return    describe the return value
 */

wBool_t wMenuAction(
    wControl_p m,
    const char * label )
{
    // wMenuItem_p mi;
    // wMenuToggle_p mt;
    // for ( mi = m->first; mi != NULL; mi = (wMenuItem_p)mi->next ) {
    // 	if ( strcmp( mi->oc.parent, label ) == 0 ) {
    // 		switch( MITEMTYPE( mi )) {
    // 		case M_SEPARATOR:
    // 			break;
    // 		case M_PUSH:
    // 			if ( ((wMenuPush_p)mi)->enabled == FALSE )
    // 				wBeep();
    // 			else
    // 				((wMenuPush_p)mi)->action( ((wMenuPush_p)mi)->data );
    // 			break;
    // 		case M_TOGGLE:
    // 			mt = (wMenuToggle_p)mi;
    // 			if ( mt->enabled == FALSE ) {
    // 				wBeep();
    // 			} else {
    // 				wMenuToggleSet( mt, !mt->set );
    // 				mt->action( mt->data );
    // 			}
    // 			break;
    // 		case M_MENU:
    // 			break;
    // 		case M_LIST:
    // 			break;
    // 		default:
    // 			/*fprintf(stderr, "Oops: wMenuAction\n");*/
    // 		break;
    // 		}
    // 		return TRUE;
    // 	}
    // }
    printf("%s:%d Not implemented!", __FILE__, __LINE__);
    return FALSE;
}
