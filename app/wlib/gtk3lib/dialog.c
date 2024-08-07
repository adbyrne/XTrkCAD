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

#include "gtkint.h"
#include "i18n.h"

#include "resources.h"

#define BASICBUILDER_RESOURCE "basicdialog"

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
    char *winSize = wPrefGetStringBasic(name, "size");
    char* winPos = wPrefGetStringBasic(name, "pos");
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

    /** /todo check screen size  */

    if (width != 0 && height != 0) {
        gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    }

    if (x != 0 && y != 0) {
        gtk_window_move(GTK_WINDOW(window), x, y);
    }
}

static
SaveWindowSizePos(GtkWidget* window, wControl_p control)
{
    gint width;
    gint height;
    gint x;
    gint y;
    gchar* value;

    gtk_window_get_size(GTK_WINDOW(window), &width, &height);

    value = g_strdup_printf("%ld %ld", width, height);
    wPrefSetString(control->name, "size", value);
    g_free(value);

    gtk_window_get_position(GTK_WINDOW(window), &x, &y);
    value = g_strdup_printf("%ld %ld", x, y);
    wPrefSetString(control->name, "pos", value);
    g_free(value);

}

/**
 * React on response signal initiated when user presses one of the dialog
 * default buttons. Selecting Help causes the help function to be executed. 
 * In all other cases the callback for the dialog is used.
 * 
 * \param self          GTK dialog
 * \param response_id   GTK button response id
 * \param dialog        wlib dialog definition
 */

void
response_signal(GtkDialog* self, gint response_id, wControl_p dialog)
{
    winProcEvent event = 0;

    SaveWindowSizePos(self, dialog); 
    
    switch (response_id) {
    case GTK_RESPONSE_OK:
        event = wAccept_e;
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_DELETE_EVENT:
        event = wCancel_e;
        break;
    case GTK_RESPONSE_HELP:
        wHelp(dialog->name);
        return;
    }

    dialog->attributes.window.winProc(dialog, event, NULL, NULL);
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
    }
    else {
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

/**
 * Load a window definition from a file and initialize the .
 * 
 * \param window
 * \param nameStr
 * \param option
 * \return 
 */

static GtkWidget *
CreateWindowFromBuilder( wControl_p window, const char *nameStr, long option )
{
    GtkWidget* dialog;
    GtkBuilder* builder;
    char* tempStr = NULL;
    const char* containerName = NULL;
    gchar* resourcePath;
     
    if (option & DO_FILESYSTEM) {
        // in case filename is given, load builder and create a name from 
        // the base filename without extension
        resourcePath = g_strdup(nameStr);
        builder = gtk_builder_new_from_file(resourcePath);
        tempStr = g_path_get_basename(resourcePath);
        tempStr[strlen(tempStr) - 3] = '\0';
        nameStr = tempStr;
        containerName = nameStr;
    }
    else {
        containerName = (option & BO_USEBUILDER ? nameStr : BASICBUILDER_RESOURCE);
        resourcePath = g_strconcat(XTRKCAD_RESOURCE_PATH,
            containerName,
            ".ui",
            NULL);
        builder = gtk_builder_new_from_resource(resourcePath);
    }

    dialog = GTK_WIDGET(gtk_builder_get_object(builder, containerName));
    g_free(resourcePath);
    resourcePath = NULL;

    window->name = g_strdup(nameStr);
    window->attributes.window.builder = builder;
    window->widget = dialog;
    g_free(tempStr);

    return(dialog);
}

/**
 * Create a dialog window from a XML ui definition.
 * 
 * ### Usage in dialogs
 *
 * - Generated: used to load the basic dialog 
 * - Builder: yes
 *
 * ### Options
 * DO_FILESYSTEM
 * : nameStr is the full path for a XML ui file with "ui" extension
 * BO_USEBUILDER
 * : nameStr is a builder file in the resources that will be used to 
 * create the complete dialog
 * default
 * : the dialog frame including a grid is created from a default ui definition
 *  
 * \param parent    IN  parent window for new dialog
 * \param helpStr   IN  help topic 
 * \param titleStr  IN  dialog title
 * \param nameStr   IN  dialog id
 * \param option    IN  see above
 * \param winProc   IN dialog procedure
 * \param attributes      IN  user attributes for dialog procedure
 * \return          dialog handle on success, NULL on failure
 */

wControl_p
wWinDialogCreate(wControl_p parent,
    const char* helpStr,
    const char* titleStr,
    const char* nameStr,
    long option,
    wWinCallBack_p winProc,
    void* attributes)
{
    GtkWidget* dialog;
    GtkWidget* parentWindow;
    struct window* dcontrol;

    wControl_p winDialog = wlibControlNew(W_DIALOG, parent, helpStr, attributes);
    dcontrol = CONTROL_GET_ATTRIBUTES_PTR(winDialog, window);

    dialog = CreateWindowFromBuilder(winDialog, nameStr, option);

    if (!dialog)
    {
        GString* errorMessage = g_string_new("Dialog id is not: ");
        g_string_append_printf(errorMessage, "%s", nameStr);
        wNoticeEx(NT_ERROR,
            errorMessage->str,
            "OK",
            NULL);
        g_string_free(errorMessage, TRUE);
        return(NULL);
    }

    if (parent == NULL) {
        parentWindow = wlibAppWinGetMain();
    }
    else {
        parentWindow = parent->widget;
    }

    RestoreWindowSizePos(dialog, nameStr);

    if (option & F_CENTER) {
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }

    g_signal_connect(G_OBJECT(dialog), "response",
        G_CALLBACK(response_signal), winDialog);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parentWindow));
    gtk_window_set_title(GTK_WINDOW(dialog), titleStr);

    gtk_widget_show(dialog);
    dcontrol->option = option & BO_USEBUILDER;
    dcontrol->winProc = winProc;

    return(winDialog);
}

/**
 * Create a new popup window. Deaded as a shim for earlier implementation
 * \todo replace in main application
 *
 * \param parent    IN Parent window (may be NULL)
 * \param x         IN Initial window width
 * \param y         IN Initial window height
 * \param helpStr   IN Help topic string
 * \param labelStr  IN Window title
 * \param nameStr   IN Window name
 * \param option    IN Options
 * \param winProc   IN call back function
 * \param attributes      IN User context information
 * \return    handle for new window
 */

static wWin_p wWinPopupCreate(
    wWindow_p parent,
    wWinPix_t x,
    wWinPix_t y,
    const char* helpStr,
    const char* labelStr,
    const char* nameStr,
    long option,
    wWinCallBack_p winProc,
    void* attributes)
{
    wWin_p win;

    //if (parent == NULL) {
    //    if (gtkMainW == NULL) {
    //        abort();
    //    }

    //    parent = gtkMainW;
    //}

    printf("%s:%d not implemented\n", __FILE__, __LINE__);

    //win = wWinCommonCreate(parent, W_POPUP, x, y, labelStr, nameStr, option,
    //    winProc, attributes);
    return win;
}

