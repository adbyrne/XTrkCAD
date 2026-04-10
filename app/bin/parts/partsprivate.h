/**
 * \file   partsprivate.h
 * \brief
 */

#ifndef partsprivate_h
#define partsprivate_h

#include <gtk/gtk.h>
#include <glib.h> // For G_TYPE_STRING, G_TYPE_INT, etc.

#include "parts.h"


// Define column indices for clarity and maintainability
typedef enum {
	COLUMN_DESCRIPTION,
	COLUMN_MANUFACTURER,
	COLUMN_PARTNO,
	COLUMN_PRICE,
	COLUMN_INVALIDPRICE,
	COLUMN_COUNT,
	NUM_COLUMNS // Keep this last to easily get the number of columns
} PartListColumn;

#endif
