/**
 * \file   notebook.c
 * \brief
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

#include <wlib.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>

#include "gtkint.h"

int
wNoteBookGetActivePage(wControl_p notebook)
{
	g_assert(notebook->type == B_NOTEBOOK);

	return(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook->widget)));
}

/**
 * Create a notebook
 *
 * ### Usage in dialogs
 *
 * - runtime not
 * - builder supported
 *
 * ### Options
 *
 * ### CSS
 *
 * \param IN parent		Handle of parent window
 * \param IN labelStr		identifier
 * \param IN activePage	page opened at creation
 * \param IN flags		unused
 * \return handle for created widget
 */

wControl_p wNotebookCreate(
        wControl_p	parent,
        const char* labelStr,
        unsigned	activePage,
        long flags)
{
	wControl_p b;

	b = wlibControlNew(B_NOTEBOOK, parent, NULL, NULL);

	if (ISDEFINEDINBUILDER(parent)) {
		b->widget = wlibWidgetFromIdWarn(parent, labelStr);
		if (b->widget) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(b->widget), activePage);
		}
	} else {
		g_assert(FALSE);
	}

	return b;
}


