/** \file recentuse.c
 * Recently used list management
 * \todo This is not only "recent use" but any type of menu list
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

#define WLISTITEM	"wListItem"		/**< id for object context */
#define WLISTMENU	"wListMenu"		/**< id for reference to main list context */

struct listentry {					/**< context of items in recently used list */
	GtkWidget* menuentry;			// text label for menu
	const char* context;			// application context
};

/**
 * Update the MRU list.
 *
 * \param ml	IN the list
 */

static void
UpdateMenuList(struct recentuse *ru)
{
	int current = MRUGetCount(ru->mrulist);

	if (current == 0) {
		//gtk_menu_item_set_label(GTK_MENU_ITEM(ru->widgets[0]), _("Empty List"));
		//gtk_widget_set_sensitive(ru->widgets[0], FALSE);
		gtk_widget_show(ru->emptyList);
	} else {
		gtk_widget_hide(ru->emptyList);


		for (int i = 0; i < current; i++) {
			struct listentry* entry = MRUGetNth(ru->mrulist, i);

			// re-order the menu elements
			g_object_ref(entry->menuentry);
			gtk_container_remove(GTK_CONTAINER(ru->parentMenu->widget),  entry->menuentry);
			gtk_menu_shell_append(GTK_MENU_SHELL(ru->parentMenu->widget), entry->menuentry);
			g_object_unref(entry->menuentry);

			gtk_widget_set_sensitive(entry->menuentry, TRUE);
			gtk_widget_show(entry->menuentry);
			//g_object_set_data(G_OBJECT(ru->widgets[i]), WLISTITEM, entry);
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
	//// pointer to the list item
	//struct listentry *item = g_object_get_data(G_OBJECT(widget), WLISTITEM);
	//struct recentuse *list = g_object_get_data(G_OBJECT(widget), WLISTMENU);

	//if (list->action) {
	//	(*list->action)(0,
	//	                gtk_menu_item_get_label(GTK_MENU_ITEM(widget)),
	//	                item->context);
	//}

	//// update order of elements in list
	//list->elements = g_slist_remove(list->elements, item );
	//list->elements = g_slist_prepend(list->elements, item );
	//ShowMenuList(list);
}

GtkWidget*
CreateEntry(const char* label, int state)
{
	GtkWidget* newItem = gtk_menu_item_new_with_label(label);
	gtk_widget_set_sensitive(newItem, state);
//	gtk_widget_show(newItem);

	return(newItem);
}

/**
 * Create the widget list for recently used. The array
 * is allocated and the menu entries are created.
 *
 * \param list	IN/OUT list pointer
 * \param max	IN maximum number of elements
 */

/**
 * Create a list menu entry
 *
 * \param m 		IN menu to be extended
 * \param helpStr 	IN reference into help
 * \param max 		IN maximum number of elements
 * \param action 	IN callback function
 * \return menu entry
 */

wControl_p wMenuListCreate(
        wControl_p m,
        const char* helpStr,
        int max,
        wMenuListCallBack_p action)
{
	struct recentuse *ru;
	wControl_p ml = wlibControlNew(M_RECENTUSE, m, helpStr, NULL);
	ru = CONTROL_GET_ATTRIBUTES_PTR(ml, recentuse);
	ru->action = action;
	ru->parentMenu = m;
	ru->mrulist = MRUCreate(max);

	// create placeholder for empty list
	ru->emptyList = CreateEntry(_("Empty list"), FALSE );
	gtk_menu_shell_append(GTK_MENU_SHELL(ru->parentMenu->widget),ru->emptyList);

	return(ml);
}

/**
 * Push a new entry to the list. If list is full, the last entry is removed.
 *
 * \param list	Menu list to be modified
 * \param label	label of new entry
 * \param context	context for new entry
 */

static int
PushListEntry(wControl_p list, const char* label, const char* context)
{
	struct recentuse *ru = CONTROL_GET_ATTRIBUTES_PTR(list, recentuse);
	struct listentry *newEntry;
	char* name;

	newEntry = g_malloc0(sizeof( struct listentry));
	newEntry->context = context;
	newEntry->menuentry = CreateEntry(label, TRUE);
	gtk_menu_shell_append(GTK_MENU_SHELL(ru->parentMenu->widget),
	                      newEntry->menuentry);

	name = g_strdup(wlibConvertInput(label));
	MRUTouchEntry(ru->mrulist, name, newEntry);

	return(MRUGetCount(ru->mrulist));
}

/**
 * Add a new item to a list of menu entries. The new item is added to the top
 * of the list. In case the maximum number of items is reached the last item
 * is removed.
 *
 * \param ml 		IN handle for the menu list - the placeholder item
 * \param index 	IN position of new menu item
 * \param labelStr 	IN the menu label for the new item
 * \param context	IN application context for the new item
 * \return
 */

void wMenuListAdd(
        wControl_p ml,
        int index,
        const char* labelStr,
        const void* context)
{
	struct recentuse *ru = CONTROL_GET_ATTRIBUTES_PTR(ml, recentuse);
	PushListEntry(ml, labelStr, context);

	UpdateMenuList(ru);
}


void wMenuListDelete(
        wControl_p ml,
        const char* labelStr)
{
	struct recentuse* ru = CONTROL_GET_ATTRIBUTES_PTR(ml, recentuse);
	MRUList *elements = ru->mrulist;
	struct listentry* entry;

	entry = MRURemoveEntry(elements, labelStr);

	if (entry) {
		gtk_widget_destroy(entry->menuentry);
		g_free(entry);
	}

	UpdateMenuList(ru);
}

/**
 * Get the label and the application context of a specific menu list item
 *
 * \param ml 	IN menu list
 * \param index IN item within list
 * \param context	OUT	application context
 * \return    item label
 */


/**  \TODO: Test saving of recent file list to preferences */
const char*
wMenuListGet(wControl_p ml, unsigned int index, void** attributes)
{
	//struct recentuse* ru = CONTROL_GET_ATTRIBUTES_PTR(ml, recentuse);

	//if (index < ru->current) {
	//	gpointer data = g_slist_nth_data(ru->elements, index);
	//	if (attributes) {
	//		*attributes = (void *)((struct listentry*)data)->context;
	//	}
	//	return(((struct listentry*)data)->label);
	//}
	return(NULL);
}

/**
 * Remove all items from menu list
 *
 * \param ml 	IN menu item list
 */
void wMenuListClear(
        wControl_p ml)
{
	//struct recentuse* ru = CONTROL_GET_ATTRIBUTES_PTR(list, recentuse);
	printf("%s:%d Not implemented!", __FILE__, __LINE__);
}
