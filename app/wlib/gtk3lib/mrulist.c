/**
 * \file   mrulist.c
 * \brief	use a doubly linked list for most-recent-used lists
 *
 * \author Martin Fischer with thanks to claude.ai
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "gtkint.h"

#include <glib.h>

typedef struct {
	char* label;
	void* userdata;
} MRUEntry;

/**
 *  Initialize list.
 *
 * \param max_capacity, -1 for unlimited (almost)
 *
 * \return handle for list
 */

MRUList* MRUCreate(int max_capacity)
{
	MRUList* list = g_malloc(sizeof(MRUList));
	list->elements = g_queue_new();
	list->max_capacity = max_capacity;
	return list;
}


static gint
labelcmp(const void* ptr1, const char* label)
{
	const MRUEntry* entry1 = (MRUEntry *)ptr1;

	return(g_strcmp0(entry1->label, label));
}

/**
 * add entry and move to front if it already exists.
 *
 * \param list      list handle
 * \param label     unique identifier for new element
 * \param entry     pointer to be put into the list, owned by caller
 *
 * \return pointer to data if element was removed, ie. when capacity was reached, NULL otherwise
 */

#define IS_LIST_FULL(mrulist) (mrulist->max_capacity != -1 && (int)g_queue_get_length(mrulist->elements) > mrulist->max_capacity)

void *MRUTouchEntry(MRUList* list, const char* label, void *entry)
{
	if (!list || !label) { return(NULL); }

	// check for existance of entry
	GList* link = g_queue_find_custom(list->elements, label,
	                                  (GCompareFunc)labelcmp);

	if (link) {
		// already present, move to top
		g_queue_unlink(list->elements, link);      // remove from current position
		g_queue_push_head_link(list->elements, link);  // and add to top
	} else {
		// new entry, create and insert at top
		MRUEntry *new_entry = g_malloc0(sizeof(MRUEntry));
		new_entry->label = g_strdup(label);
		new_entry->userdata = entry;
		g_queue_push_head(list->elements, new_entry);

		// remove oldest if capacity is reached
		if (IS_LIST_FULL(list)) {
			MRUEntry *oldest = (MRUEntry *)g_queue_pop_tail(list->elements);
			void* userdata = oldest->userdata;
			g_free(oldest);
			return(userdata);
		}
	}
	return(NULL);
}

void* MRUAppendEntry(MRUList* list, const char* label, void* entry)
{
	if (!list || !label) { return(NULL); }

	// check for existance of entry
	const GList* link = g_queue_find_custom(list->elements, label,
	                                        (GCompareFunc)labelcmp);

	if (!link) {
		// new entry, create and insert at end
		MRUEntry* new_entry = g_malloc0(sizeof(MRUEntry));
		new_entry->label = g_strdup(label);
		new_entry->userdata = entry;

		g_queue_push_tail(list->elements, new_entry);

		// remove oldest if capacity is reached
		if (IS_LIST_FULL(list)) {
			MRUEntry* oldest = (MRUEntry*)g_queue_pop_head(list->elements);
			void* userdata = oldest->userdata;
			g_free(oldest);
			return(userdata);
		}
	}
	return(NULL);
}

/**
 * get most recent entry.
 *
 * \param list  list handle
 * \return      user pointer
 */
void * MRUGetRecent(MRUList* list)
{
	if (!list || g_queue_is_empty(list->elements)) { return NULL; }

	MRUEntry* element = (MRUEntry*)g_queue_peek_head(list->elements);
	return (void *)element->userdata;
}

/**
 * get entry at position.
 *
 * \param list  list handle
 * \param index position of data
 * \return
 */

void * MRUGetNth(MRUList* list, int index)
{
	if (!list) {
		return NULL;
	}

	MRUEntry* element = (MRUEntry*)(g_queue_peek_nth(list->elements, index));

	return (void *)element->userdata;
}

/**
 * return number of entries in list
 *
 * \param list  list handle
 * \return      count of entries
 */

int MRUGetCount(MRUList* list)
{
	if (!list) { return 0; }
	return g_queue_get_length(list->elements);
}

/**
 * remove entry specified by its name
 *
 * \param list  list handle
 * \param label identifier of element to remove
 *
 * \return  user pointer that was stored in list, NULL if element not found
 */


void * MRURemoveEntry(MRUList* list, const char* label)
{
	if (!list || !label) { return FALSE; }

	GList* link = g_queue_find_custom(list->elements, label, labelcmp);
	if (link) {
		MRUEntry* listelement = (MRUEntry*)link->data;
		void* elementdata = listelement->userdata;

		g_free(listelement->label);
		g_queue_delete_link(list->elements, link);

		return elementdata;
	}
	return NULL;
}

/**
 * clear list. Removes all elements from list and frees the allocated memory.
 * NOTE: allocations for user data have to be freed before calling this function
 *
 * \param list  list handle
 */

free_element(gpointer data)
{
	MRUEntry* queueelement = (MRUEntry*)data;

	if (data) {
		// printf("label: %s\n", queueelement->label);
		g_free(queueelement->label);
	}
}


void MRUClear(MRUList* list)
{
	if (!list) { return; }

	// free all elements

	//printf("MRUClear: %d elements\n", MRUGetCount(list));
	g_queue_foreach(list->elements, (GFunc)free_element, NULL);
	g_queue_clear(list->elements);
}

//
/**
 * Destroy the queue and free the allocated memory.
 *
 * \param list  list handle
 */
void MRUDestroy(MRUList* list)
{
	if (!list) { return; }

	MRUClear(list);
	g_queue_free(list->elements);
	g_free(list);
}

