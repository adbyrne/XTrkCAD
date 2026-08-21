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

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include "gtkint.h"
#include <wlib.h>

int
wNoteBookGetActivePage(wControl_p notebook)
{
	g_assert(notebook->type == B_NOTEBOOK);

	return(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook->widget)));
}

void
wNoteBookSetActivePage(wControl_p notebook, int page)
{
	g_assert(notebook->type == B_NOTEBOOK);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook->widget), page);
}

void
wNoteBookShowTabs(wControl_p notebook, wBool_t show)
{
	g_assert(notebook->type == B_NOTEBOOK);

	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook->widget), show != FALSE);
}

static void
NotebookSwitchPage(GtkNotebook *notebook, GtkWidget *page, guint page_num,
                   gpointer data)
{
	wControl_p control = (wControl_p)data;
	struct notebook *nbAttr = CONTROL_GET_ATTRIBUTES_PTR(control, notebook);

	if (nbAttr->action) {
		nbAttr->action((long)page_num, control->context);
	}
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
 * \param[in] parent		Handle of parent window
 * \param[in] labelStr		identifier
 * \param[in] activePage	page opened at creation
 * \param[in] flags		unused
 * \param[in] action		called with the new page number on tab switch
 * \param[in] context	passed through to action
 * \return handle for created widget
 */

wControl_p wNotebookCreate(
        wControl_p	parent,
        const char* labelStr,
        unsigned	activePage,
        long flags,
        wChoiceCallBack_p action,
        void *context)
{
	wControl_p b;
	struct notebook *nbAttr;

	b = wlibControlNew(B_NOTEBOOK, parent, NULL, NULL);
	nbAttr = CONTROL_GET_ATTRIBUTES_PTR(b, notebook);
	nbAttr->action = action;
	b->context = context;

	if (ISDEFINEDINBUILDER(parent)) {
		b->widget = wlibWidgetFromIdWarn(parent, labelStr);
		if (b->widget) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(b->widget), activePage);
			g_signal_connect_after(b->widget, "switch-page",
			                        G_CALLBACK(NotebookSwitchPage), (gpointer)b);
		}
	} else {
		g_assert(FALSE);
	}

	return b;
}


