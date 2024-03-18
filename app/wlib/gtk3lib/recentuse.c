/** \file recentuse.c
 * Recently used list management
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
#define WLISTMENU	"wListMenu"		/**< id for reference to main list data */

struct wMenuList_t {
    struct wObjCommon oc;
    struct menuObjCommon mc;
    wMenuListCallBack_p action;

    unsigned max;				// maximum members
    unsigned current;			// current number of members
    GSList *elements;			// list of elements
    GtkWidget** widgets;		// display widgets
};

typedef struct wMenuListItem_t* wMenuListItem_p;

struct entry {					/**< data of items in recently used list */
    char* label;				// text label for menu
    const char* data;			// application data
};

/*-----------------------------------------------------------------*/

/**
 * Update the MRU list.
 *
 * \param ml	IN the list
 */

static void
ShowMenuList(wMenuList_p ml)
{
    if (ml->current == 0) {
        gtk_menu_item_set_label(GTK_MENU_ITEM(ml->widgets[0]), "Empty List");
        gtk_widget_set_sensitive(ml->widgets[0], FALSE);
        gtk_widget_show(ml->widgets[0]);
    }
    else {
        // iterate over ml: update labels
        for (unsigned i = 0; i < ml->current; i++) {
            struct entry* entry = g_slist_nth_data(ml->elements, i);
            gtk_menu_item_set_label(GTK_MENU_ITEM(ml->widgets[i]),
                                    entry->label);
            gtk_widget_set_sensitive(ml->widgets[i], TRUE);
            gtk_widget_show(ml->widgets[i]);
            g_object_set_data(G_OBJECT(ml->widgets[i]), WLISTITEM, entry);
        }

    }
}

/**
 * Signal handler for clicking onto a menu list item. After action handler is
 * called the activated element is moved to the top of the list.
 *
 * \param widget IN the GtkWidget
 * \param value  IN the menu list item
 * \return
 */

static void ActivateListMenuItem(
    GtkWidget* widget,
    gpointer value)
{
    // pointer to the list item
    struct entry *item = g_object_get_data(G_OBJECT(widget), WLISTITEM);
    wMenuList_p list = g_object_get_data(G_OBJECT(widget), WLISTMENU);

    GSList* actElement;

    if (list->action) {
        (*list->action)(0,
                        gtk_menu_item_get_label(GTK_MENU_ITEM(widget)),
                        item->data);
    }

    // update order of elements in list
    list->elements = g_slist_remove(list->elements, item );
    list->elements = g_slist_prepend(list->elements, item );
    ShowMenuList(list);
}

/**
 * Create for the recently used menu entries. As this is
 * a GSList simple initialization is needed only.
 *
 * \param list	IN/OUT list pointer
 * \param max	IN maximum number of elements (unused)
 */
static void
CreateListElements(GSList *list, unsigned int max)
{
    list  = NULL;
}

/**
 * Create the widget list for recently used. The array
 * is allocated and the menu entries are created.
 *
 * \param list	IN/OUT list pointer
 * \param max	IN maximum number of elements
 */

static void
CreateListEntries(wMenuList_p list, unsigned int  max)
{
    // pre-allocate the required menu entries
    list->widgets = g_malloc0(sizeof(GtkWidget*) * max);
    for (unsigned int i = 0; i < max; i++) {
        list->widgets[ i ] = gtk_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(list->mc.menu_item),
                              list->widgets[i]);

        g_signal_connect(G_OBJECT(list->widgets[i]), "activate",
                         G_CALLBACK(ActivateListMenuItem), NULL);

        // initially hidden
        gtk_widget_set_sensitive(list->widgets[i], FALSE);
        gtk_widget_hide(list->widgets[i]);

        // store pointer back to the list config element
        g_object_set_data(G_OBJECT(list->widgets[i]), WLISTMENU, list);
    }
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
    const char* helpStr,
    int max,
    wMenuListCallBack_p action)
{
    wMenuList_p ml = g_malloc0(sizeof(struct wMenuList_t));
    ml->action = action;
    ml->mc.menu_item = m->menu;
    ml->elements = NULL;
    ml->max = max;

    CreateListElements(ml->elements, max);
    CreateListEntries(ml, max);

    ShowMenuList(ml);

    return (wMenuList_p)ml;
}

/**
 * Remove an element from list without freeing the element's memory.
 */
static struct entry *
RemoveEntry(GSList* list, unsigned element)
{
    GSList* old = NULL;
    struct entry* entry;

    old = g_slist_nth(list, element);
    list = g_slist_remove_link(list, old);

    entry = g_slist_nth_data(old, 0);

    g_slist_free_1(old);

    return(entry);
}

/**
 * Remove an element from the list and free the related memory.
 *
 * \param list		list of recently used items
 * \param element	index of the element to be removed
 */

static void
FreeEntry(GSList* list, unsigned element)
{
    struct entry *entry = RemoveEntry(list, element);

    g_free(entry->label);
    g_free(entry);
}

/**
 * Push a new entry to the list. If list is full, the last entry is removed.
 *
 * \param list	Menu list to be modified
 * \param label	label of new entry
 * \param data	data for new entry
 */

static int
PushListEntry(wMenuList_p list, const char* label, const char* data)
{
    struct entry *newEntry;
    newEntry = g_malloc0(sizeof( struct entry));

    if (g_slist_length(list->elements) == list->max) {
        //remove entry
        FreeEntry(list->elements, list->max-1);
    }

    newEntry->label = g_strdup(wlibConvertInput(label));
    newEntry->data = data;

    list->elements = g_slist_prepend(list->elements, newEntry);

    return(g_slist_length(list->elements));
}

/**
 * Add a new item to a list of menu entries. The new item is added to the top
 * of the list. In case the maximum number of items is reached the last item
 * is removed.
 *
 * \param ml 		IN handle for the menu list - the placeholder item
 * \param index 	IN position of new menu item
 * \param labelStr 	IN the menu label for the new item
 * \param data 		IN application data for the new item
 * \return
 */

void wMenuListAdd(
    wMenuList_p ml,
    int index,
    const char* labelStr,
    const void* data)
{
    ml->current = PushListEntry(ml, labelStr, data);

    ShowMenuList(ml);
}

/**
 * Remove the menu entry identified by a given label.
 *
 * \param ml IN menu list
 * \param labelStr IN label string of item
 */

void wMenuListDelete(
    wMenuList_p ml,
    const char* labelStr)
{
    printf("%s:%d Not implemented!", __FILE__, __LINE__);
}

/**
 * Get the label and the application data of a specific menu list item
 *
 * \param ml 	IN menu list
 * \param index IN item within list
 * \param data	OUT	application data
 * \return    item label
 */

const char*
wMenuListGet(wMenuList_p ml, unsigned int index, void** data)
{
    printf("%s:%d Not implemented!", __FILE__, __LINE__);

    return(NULL);
}

/**
 * Remove all items from menu list
 *
 * \param ml 	IN menu item list
 */

void wMenuListClear(
    wMenuList_p ml)
{
    printf("%s:%d Not implemented!", __FILE__, __LINE__);
}
