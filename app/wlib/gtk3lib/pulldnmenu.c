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


//typedef struct{	mtype_e mtype;			/**< menu entry type */
//				GtkWidget *menu_item;
//				wMenu_p parentMenu;
//		} MOBJ_COMMON; 					/**< menu item specific data */

// common data for all menu items
struct wMenuItem_t {
    struct wObjCommon oc;
    struct menuObjCommon mc;
};
typedef struct wMenuItem_t * wMenuItem_p;

// extend common data for push button item
struct wMenuPush_t {
    struct wObjCommon oc;
    struct menuObjCommon mc;
    wMenuCallBack_p action;
    void *data;
};

// extend common data for toggle button item
struct wMenuToggle_t {
    struct wObjCommon oc;
    struct menuObjCommon mc;
    wMenuCallBack_p action;
    void *data;
};

//extend common data for radio button item
struct wMenuRadio_t {
    struct wObjCommon oc;
    struct menuObjCommon mc;
    wMenuCallBack_p action;
    void *data;
};


// a few macros to make access to members easier

#define MMENUITEM( ptr ) 	((ptr)->mc.menu_item)
#define MPARENT( ptr ) 	((ptr)->mc.parentMenu)
#define MITEMTYPE( ptr )	((ptr)->mc.mtype)

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

    switch MITEMTYPE( m ) {
    case M_PUSH:
        ((wMenuPush_p)m)->action( ((wMenuPush_p)m)->data );
        break;
    case M_TOGGLE:
        mt = (wMenuToggle_p)m;
        mt->action( mt->data );
        break;
    case M_RADIO:
        /* NOTE: action is only called when radio button is activated,
        not when deactivated */
        if( gtk_check_menu_item_get_active((GtkCheckMenuItem *)widget ) == TRUE )
            ((wMenuRadio_p)m)->action( ((wMenuRadio_p)m)->data );
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
    wMenu_p m,
    wMenuItem_p mi,
    mtype_e mtype,
    const char * helpStr,
    const char * labelStr,
    int acclKey )
{
    mi->oc.helpTopic = g_strdup(helpStr);
    mi->oc.labelStr = g_strdup(labelStr);

    MITEMTYPE( mi )= mtype;

    switch ( mtype ) {
    case M_SEPARATOR:
        MMENUITEM( mi ) = gtk_separator_menu_item_new();
        break;
    case M_TOGGLE:
        MMENUITEM( mi ) = gtk_check_menu_item_new_with_mnemonic(
                              wlibConvertInput(mi->oc.labelStr));
        g_signal_connect(MMENUITEM(mi), "toggled", G_CALLBACK(pushMenuItem),
                         mi);
        break;
    case M_RADIO:
        MMENUITEM( mi ) = gtk_radio_menu_item_new_with_mnemonic(m->radioGroup,
                          wlibConvertInput(mi->oc.labelStr));
        m->radioGroup = gtk_radio_menu_item_get_group (
                            GTK_RADIO_MENU_ITEM (MMENUITEM( mi )));
        g_signal_connect(MMENUITEM(mi), "activate", G_CALLBACK(pushMenuItem),
                         mi);
        break;
    case M_PUSH:
        MMENUITEM( mi ) = gtk_menu_item_new_with_mnemonic(
                              wlibConvertInput(mi->oc.labelStr));
        g_signal_connect(MMENUITEM(mi), "activate",G_CALLBACK(pushMenuItem),
                         mi);
        break;
    case M_MENU:
        MMENUITEM(mi) = gtk_menu_item_new_with_mnemonic(
                            wlibConvertInput(mi->oc.labelStr));
        break;
    default:
        g_abort();
        break;
    }

    if (MMENUITEM( mi )) {
        if (acclKey) {
            setAcclKey(MMENUITEM(mi), acclKey);
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(m->menu), MMENUITEM( mi ));
        gtk_widget_show(GTK_WIDGET(MMENUITEM(mi)));
    }

    if (helpStr != NULL) {
//		wlibAddHelpString( MMENUITEM( mi ), helpStr );
    }
    MPARENT( mi ) = m;
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

wMenuRadio_p wMenuRadioCreate(
    wMenu_p m,
    const char * helpStr,
    const char * labelStr,
    long acclKey,
    wMenuCallBack_p action,
    void 	*data )
{
    wMenuRadio_p mi = g_malloc(sizeof(struct wMenuRadio_t));

    CreateMenuItem( m, (wMenuItem_p)mi, M_RADIO, helpStr, labelStr, acclKey );

    mi->action = action;
    mi->data = data;
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

wMenuPush_p wMenuPushCreate(
    wMenu_p m,
    const char * helpStr,
    const char * labelStr,
    long acclKey,
    wMenuCallBack_p action,
    void 	*data )
{
    wMenuPush_p mi = g_malloc(sizeof(struct wMenuPush_t));

    CreateMenuItem( m, (wMenuItem_p)mi, M_PUSH, helpStr, labelStr, acclKey);
    mi->action = action;
    mi->data = data;

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
    wMenu_p mi = g_malloc(sizeof(struct wMenu_t));
    mi->radioGroup = NULL;

    CreateMenuItem( m, (wMenuItem_p)mi, M_MENU, helpStr, labelStr, 0);

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
    wMenuItem_p mi = g_malloc(sizeof(struct wMenuItem_t ));

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

wMenuToggle_p wMenuToggleCreate(
    wMenu_p m,
    const char * helpStr,
    const char * labelStr,
    long acclKey,
    wBool_t set,
    wMenuCallBack_p action,
    void * data )
{
    wMenuToggle_p mt = g_malloc(sizeof(struct wMenuToggle_t));

    CreateMenuItem(m, (wMenuItem_p)mt, M_TOGGLE, helpStr, labelStr, acclKey );

    mt->action = action;
    mt->data = data;
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
    return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(MMENUITEM( mt )));
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
    wBool_t oldState;
    if (mt==NULL) return 0;

    oldState = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(MMENUITEM( mt )));
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(MMENUITEM( mt )), set );

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
    wMenuToggle_p mt,
    wBool_t enable )
{
    gtk_widget_set_sensitive ( GTK_WIDGET(MMENUITEM( mt )), enable );
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

wMenu_p wMenuBarAdd(
    wWindow_p w,
    const char * helpStr,
    const char * labelStr )
{
    wMenu_p m;
    GtkWidget *menuItem;
    static wMenu_p m0 = NULL;

    m = g_malloc(sizeof(struct wMenu_t));
    m->oc.helpTopic = g_strdup(helpStr);
    m->oc.labelStr = g_strdup(labelStr);
    m->mmtype = MM_SUBMENU;
    m->radioGroup = NULL;

    menuItem = gtk_menu_item_new_with_mnemonic(labelStr);

    m->menu = gtk_menu_new();

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuItem), m->menu );
    gtk_menu_shell_append(GTK_MENU_SHELL(w->menubar), menuItem);

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
