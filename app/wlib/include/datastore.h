#ifndef DATASTORE_H
#define DATASTORE_H
#include <gtk/gtk.h>


/* custom list store interface */
struct _DataStore {
	GtkListStore* listStore;
	GCallback  selectionChanged;
	const gchar* editable;
	GCallback  edited;
};

#endif
