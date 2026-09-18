/*****************************************************************//**
 * \file   dialog.c
 * \brief  Create and handle dialogs
 *
 * \author mf
 * \date   April 2024
 *********************************************************************/


/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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
#include "glib-object.h"
#include "glib.h"

#include "gtkint.h"
#include "i18n.h"

/**
 * Place widget into the grid of a basic dialog window.
 *
 * \param parent        parent window
 * \param widget        widget to place
 * \param xPos, yPos    position
 * \param colSpan, rowSpan  size
 */

void
wlibBasicGridAttach(wControl_p parent, GtkWidget *widget, unsigned xPos,
                    unsigned yPos, unsigned colSpan, unsigned rowSpan)
{
	GtkGrid* grid = GTK_GRID(wlibWidgetFromIdWarn(parent, "layoutgrid"));

	gtk_grid_attach(grid, widget, xPos, yPos, colSpan, rowSpan);
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
}

void *wSameRowCreate(wControl_p parent, unsigned x, unsigned y)
{
	GtkGrid *grid = GTK_GRID(wlibWidgetFromIdWarn(parent, "layoutgrid"));
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_grid_attach(grid, hbox, x, y, 1, 1);
	gtk_widget_set_halign(hbox, GTK_ALIGN_START);
	gtk_widget_show(hbox);
	return hbox;
}

void wSameRowAdd(void *samerow, wControl_p ctl)
{
	GtkWidget *w = ctl->widget;
	g_object_ref(w);
	gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(w)), w);
	gtk_box_pack_start(GTK_BOX(samerow), w, FALSE, FALSE, 0);
	g_object_unref(w);
}

/**
 * Restore the window position from the INI file. Setting to 0 position or
 * size is not possible,
 *
 * \param window    window handle
 * \param name      name of window
 */

static void
RestoreWindowSizePos(GtkWidget* window, const char* name)
{
	const char *winSize = wPrefGetStringBasic(name, "size");
	const char* winPos = wPrefGetStringBasic(name, "pos");
	gchar** parsedString;
	gint x = 0;
	gint y = 0;
	gint width = 0;
	gint height = 0;

	if (winPos) {
		parsedString = g_strsplit(winPos, " ", 2);
		x = (gint)g_ascii_strtoll(parsedString[0], NULL, 10);
		y = (gint)g_ascii_strtoll(parsedString[1], NULL, 10);
		g_strfreev(parsedString);
	}

	if (winSize) {
		parsedString = g_strsplit(winSize, " ", 2);
		width = (gint)g_ascii_strtoll(parsedString[0], NULL, 10);
		height = (gint)g_ascii_strtoll(parsedString[1], NULL, 10);
		g_strfreev(parsedString);
	}

	/** \todo check screen size and clamp the size to a value smaller than the screen */

	if (width != 0 && height != 0) {
		gtk_window_set_default_size(GTK_WINDOW(window), width, height);
	}

	if (x != 0 && y != 0) {
		gtk_window_move(GTK_WINDOW(window), x, y);
	}
}

static void
SaveWindowSizePos(const GtkWidget* window, const char *name)
{
	gint width;
	gint height;
	gint x;
	gint y;
	gchar* value;

	gtk_window_get_size(GTK_WINDOW(window), &width, &height);

	value = g_strdup_printf("%d %d", width, height);
	wPrefSetString(name, "size", value);
	g_free(value);

	gtk_window_get_position(GTK_WINDOW(window), &x, &y);
	value = g_strdup_printf("%d %d", x, y);
	wPrefSetString(name, "pos", value);
	g_free(value);

}

void
wDialogSaveSizePos(wControl_p dialog)
{
	SaveWindowSizePos(dialog->widget, dialog->name);
}

/**
 * Configure a button.
 *
 * \param dialog    the dialog holding the button
 * \param id        id of the button as defined in the builder file
 * \param label     text label. If NULL the button is hidden
 */

static void
ConfigureButton(struct window* dialog, const char* id, const char* label)
{
	GtkWidget* button;

	button = GTK_WIDGET(gtk_builder_get_object(dialog->builder, id));
	if (button && label) {
		gtk_button_set_label(GTK_BUTTON(button), label);
		gtk_widget_show(button);
	} else {
		gtk_widget_hide(button);
	}
}

/**
 * Configure the control buttons (OK, Cancel, Help) of a dialog. The ids are
 * defined by the builder file used for creating the dialog. The label to be
 * placed into the button can be NULL. In that case the button is hidden.
 *
 * \param dialog        dialog
 * \param okLabel, cancelLabel, helpLabel the text labels
 */

void
wDialogButtonsConfigure(wControl_p dialog, const char* okLabel,
                        const char* cancelLabel, const char* helpLabel)
{
	ConfigureButton(&dialog->attributes.window, "id_ok", okLabel);
	ConfigureButton(&dialog->attributes.window, "id_cancel", cancelLabel);
	ConfigureButton(&dialog->attributes.window, "id_help", helpLabel);
}

static gboolean
deleteHandler(GtkDialog *win, GdkEvent *event, gpointer userdata)
{
	wControl_p dialog = (wControl_p)userdata;
	struct window *dcontrol = CONTROL_GET_ATTRIBUTES_PTR(dialog, window);

	if(dcontrol->winProc) {
		dcontrol->winProc(dialog, wClose_e, NULL,  dialog->context);
		return gtk_widget_hide_on_delete(win);
	}

	return FALSE;
}

/**
 * Create a dialog window from a XML ui definition.
 *
 * ### Usage in dialogs
 *
 * - runtime: used to load the basic dialog
 * - builder: yes
 *
 * ### Options
 * F_CENTER center new window on parent window
 * F_RESIZE user can resize the window
 *
 * \param parent    IN  parent window for new dialog
 * \param helpStr   IN  help topic
 * \param titleStr  IN  dialog title
 * \param nameStr   IN  dialog id
 * \param option    IN  see above
 * \param winProc   IN dialog procedure
 * \param context      IN  user context for dialog procedure
 * \return          dialog handle on success, NULL on failure
 */

wControl_p
wWinDialogCreate(wControl_p parent,
                 const char* helpStr,
                 const char* titleStr,
                 const char* nameStr,
                 long option,
                 wWinCallBack_p winProc,
                 void* context)
{
	GtkWidget* dialog;
	GtkWidget* parentWindow;
	struct window* dcontrol;

	wControl_p winDialog = wlibControlNew(W_DIALOG, parent, helpStr, context);
	dcontrol = CONTROL_GET_ATTRIBUTES_PTR(winDialog, window);
	dcontrol->option = option;

	if (option & F_DEFINEDINBUILDER) {
		dialog = wlibCreateWindowFromBuilder(winDialog, nameStr, option);
	} else {
		dialog = wlibCreateWindowFromBuilder(winDialog, "basicdialog", option);

		if (option & F_RESIZE) {
			gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
		}
	}


	if (!dialog) {
		GString* errorMessage = g_string_new("Dialog id is not: ");
		g_string_append_printf(errorMessage, "%s", nameStr);
		wNoticeWithIcon(NT_ERROR,
		                errorMessage->str,
		                "OK",
		                NULL);
		g_string_free(errorMessage, TRUE);
		g_assert(NULL);
		return(NULL);
	}

	winDialog->widget = dialog;
	winDialog->name = g_strdup(nameStr);

	if (parent == NULL) {
		parentWindow = wlibAppWinGetMain();
	} else {
		parentWindow = parent->widget;
	}

	RestoreWindowSizePos(dialog, nameStr);

	if (option & F_CENTER) {
		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	}

	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parentWindow));

	if (titleStr) {
		gtk_window_set_title(GTK_WINDOW(dialog), titleStr);
	}

	g_signal_connect(dialog, "delete-event", deleteHandler, winDialog);

	gtk_widget_show(dialog);
	dcontrol->winProc = winProc;

	return(winDialog);
}
