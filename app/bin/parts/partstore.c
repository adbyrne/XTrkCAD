/**
 * \file   partstore.c
 * \brief  Accss to list for parts
 *
 * \author Martin Fischer
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

#include "partsprivate.h"
#include <parts.h>
#include <wlib.h>
#include <datastore.h>

DataStore *
PartListStoreNew(void *selectChange, const char *editRenderer,
                 void (*edited)(int row, char *newvalue))
{

	DataStore* store = g_malloc(sizeof(DataStore));
	store->listStore = gtk_list_store_new(NUM_COLUMNS,
	                                      G_TYPE_STRING,
	                                      G_TYPE_STRING,
	                                      G_TYPE_STRING,
	                                      G_TYPE_STRING,
	                                      G_TYPE_BOOLEAN,
	                                      G_TYPE_INT
	                                     );

	store->selectionChanged = selectChange;
	store->editable = editRenderer;
	store->edited = edited;
	return store;
}

void
PartListAddToListbox(wControl_p list, DataStore *store)
{
	wListSetStore(list, store);
}

void
PartListStoreClear(DataStore* store)
{
	gtk_list_store_clear(store->listStore);
}

int
PartListStoreAddPart(DataStore* store, const Part* part)
{
	GtkTreeIter iter;

	gtk_list_store_append(store->listStore, &iter);
	gtk_list_store_set(store->listStore, &iter,
	                   COLUMN_DESCRIPTION, part->description,
	                   COLUMN_MANUFACTURER, part->manufacturer,
	                   COLUMN_PARTNO, part->partno,
	                   COLUMN_PRICE, part->price,
	                   COLUMN_INVALIDPRICE, part->is_invalid,
	                   COLUMN_COUNT, part->count,
	                   -1);

	// get the zero based index of the newly added row
	// GTK_TREE_MODEL() is GTK's own type-check-cast macro (through
	// GTypeInstance*); required, unavoidable GTK idiom.
	// NOLINTNEXTLINE(bugprone-casting-through-void)
	return(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store->listStore),
	                                      NULL) - 1);
}

void PartListStoreGetPart(DataStore* store, unsigned index, Part* part)
{
	GtkTreeIter iter;
	GtkTreePath* treepath = gtk_tree_path_new_from_indices(index, -1);

	// GTK_TREE_MODEL() is GTK's own type-check-cast macro; required,
	// unavoidable GTK idiom.
	// NOLINTNEXTLINE(bugprone-casting-through-void)
	gtk_tree_model_get_iter(GTK_TREE_MODEL(store->listStore), &iter,  treepath);

	// NOLINTNEXTLINE(bugprone-casting-through-void)
	gtk_tree_model_get(GTK_TREE_MODEL(store->listStore), &iter,
	                   COLUMN_DESCRIPTION, &part->description,
	                   COLUMN_MANUFACTURER, &part->manufacturer,
	                   COLUMN_PARTNO, &part->partno,
	                   COLUMN_PRICE, &(part->price),
	                   COLUMN_COUNT, &(part->count),
	                   -1
	                  );

	gtk_tree_path_free(treepath);
}


bool PartListUpdatePrice(DataStore* store, unsigned index, char * price,
                         bool invalid)
{
	GtkTreeIter iter;
	GtkTreePath* treepath = gtk_tree_path_new_from_indices(index, -1);
	unsigned wasInvalid;

	// NOLINTNEXTLINE(bugprone-casting-through-void)
	gtk_tree_model_get_iter(GTK_TREE_MODEL(store->listStore), &iter,  treepath);

	// NOLINTNEXTLINE(bugprone-casting-through-void)
	gtk_tree_model_get(GTK_TREE_MODEL(store->listStore), &iter,
	                   COLUMN_INVALIDPRICE, &wasInvalid,
	                   -1);

	gtk_list_store_set(store->listStore, &iter,
	                   COLUMN_PRICE, price,
	                   COLUMN_INVALIDPRICE, invalid,
	                   -1
	                  );

	gtk_tree_path_free(treepath);

	return(wasInvalid);
}

void PartListRemovePart(DataStore* store, unsigned index)
{

}

