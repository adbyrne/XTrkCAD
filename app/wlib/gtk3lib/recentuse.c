/** \file recentuse.c
 * menu list management
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

struct listentry {					/**< context of items in recently used list */
	GtkWidget* menuentry;			// text label for menu
	struct recentuse* recentuse;
	const char* context;			// application context
};

/**
 * Update the MRU list.
 *
 * \param ru	IN the list
 */

static void
UpdateMenuList(struct recentuse *ru)
{
	int current = MRUGetCount(ru->mrulist);

	if (current == 0) {
		gtk_menu_item_set_label(GTK_MENU_ITEM(ru->emptyList), _("Empty List"));
		gtk_widget_set_sensitive(ru->emptyList, FALSE);
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
		}

	}
}

/**
 * Signal handler for clicking onto a menu list item. After action handler is
 * called the activated element is moved to the top of the list.
 *
 * \param widget IN the GtkWidget
 * \param value  IN the menu list item
 */

static void ActivateListMenuItem(
        GtkWidget* widget,
        gpointer value)
{
	struct listentry* userdata = (struct listentry*)value;
	struct recentuse *ru = userdata->recentuse;

	if (ru->action) {
		(*ru->action)(0,
		              gtk_menu_item_get_label(GTK_MENU_ITEM(userdata->menuentry)),
		              (char *)userdata->context); // CAST_AWAY_CONST
	}

}

GtkWidget*
CreateEntry(const char* label, int state)
{
	GtkWidget* newItem = gtk_menu_item_new_with_label(label);
	gtk_widget_set_sensitive(newItem, state);

	return(newItem);
}

/**
 * Create a list menu entry
 *
 * \param m 		IN menu to be extended
 * \param helpStr 	IN reference into help
 * \param sorder 	IN sort order
 * \param max 		IN maximum number of elements
 * \param action 	IN callback function
 * \return menu entry
 */

wControl_p wMenuListCreate(
        wControl_p m,
        const char* helpStr,
        SORTORDER sorder,
        int max,
        wMenuListCallBack_p action)
{
	struct recentuse *ru;
	wControl_p ml = wlibControlNew(M_RECENTUSE, m, helpStr, NULL);
	ru = CONTROL_GET_ATTRIBUTES_PTR(ml, recentuse);
	ru->action = action;
	ru->parentMenu = m;
	ru->mrulist = MRUCreate(max);
	ru->sortorder = sorder;

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
	const char* name;

	newEntry = g_malloc0(sizeof( struct listentry));
	newEntry->context = context;
	newEntry->menuentry = CreateEntry(label, TRUE);
	newEntry->recentuse = ru;

	gtk_menu_shell_append(GTK_MENU_SHELL(ru->parentMenu->widget),
	                      newEntry->menuentry);

	g_signal_connect(newEntry->menuentry, "activate", ActivateListMenuItem,
	                 newEntry);

	name = g_strdup(wlibConvertInput(label));

	if (ru->sortorder == NEWEST_TOP) {
		MRUTouchEntry(ru->mrulist, name, newEntry);
	} else {
		MRUAppendEntry(ru->mrulist, name, newEntry);
	}

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
 * \param attributes	OUT	application context
 * \return    item label
 */

const char*
wMenuListGet(wControl_p ml, int index, void** attributes)
{
	struct recentuse* ru = CONTROL_GET_ATTRIBUTES_PTR(ml, recentuse);
	int count = MRUGetCount(ru->mrulist);

	if(index < count) {
		void* entry;

		entry = MRUGetNth(ru->mrulist, index);
		if (entry) {
			*attributes = (void*)((struct listentry*)entry)->context;
			return(((struct listentry*)entry)->context);
		}
	}
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
	struct recentuse* ru = CONTROL_GET_ATTRIBUTES_PTR(ml, recentuse);

	int count = MRUGetCount(ru->mrulist);

	for (int i = 0; i < count; i++) {

		void* user_data = MRUGetNth(ru->mrulist, i);
		struct listentry * nextEntry = (struct listentry *)user_data;

		if (nextEntry) {
			gtk_widget_destroy(nextEntry->menuentry);
			g_free(nextEntry);
		}
	}

	MRUClear(ru->mrulist);
}

int
wMenuListGetCount(wControl_p ml)
{
	g_assert(ml);

	struct recentuse* ru = CONTROL_GET_ATTRIBUTES_PTR(ml, recentuse);

	return(MRUGetCount(ru->mrulist));
}
