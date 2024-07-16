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
}

/**
 * Handle configure event. Window position and size are saved
 * 
 * \param self
 * \param event
 * \param userdata  unused
 * \return 
 */

gboolean
dialog_configure_event(GtkWidget* self, GdkEventConfigure * event, void* userdata)
{
    gchar* posString = NULL;

    posString = g_strdup_printf("%d %d", event->x, event->y);
//    wPrefSetString(((wWindow_p)userdata)->name, "pos", posString);
    g_free(posString);

    posString = g_strdup_printf("%d %d", event->width, event->height);
//    wPrefSetString(((wWindow_p)userdata)->name, "size", posString);
    g_free(posString);
    
    return FALSE;
}
/**
 * Restore the window position from the INI file. Setting to 0 position or
 * size is not possible, 
 * 
 * \param window    window handle
 * \param name      name of window 
 */
static void
RestoreWindow(GtkWidget* window, const char* name)
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
    
    switch (response_id) {
    case GTK_RESPONSE_OK:
        event = wAccept_e;
        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_DELETE_EVENT:
        event = wCancel_e;
        break;
    case GTK_RESPONSE_HELP:
        //wHelp(dialog->nameStr);
        return;
    }

    dialog->data.window.winProc(dialog, event, NULL, NULL);
} 

void
wWindowShow(wControl_p win, bool state)
{
    if (state) {
        gtk_widget_show(win->widget);
    }
    else {
        gtk_widget_hide(win->widget);
    }
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
 * BO_FILESYSTEM
 * : nameStr is the full path for a XML ui file with "ui" extension
 *  
 * \param parent    IN  parent window for new dialog
 * \param helpStr   IN  help topic 
 * \param titleStr  IN  dialog title
 * \param nameStr   IN  dialog id
 * \param option    IN  see above
 * \param winProc   IN dialog procedure
 * \param data      IN  user data for dialog procedure
 * \return          dialog handle on success, NULL on failure
 */

wControl_p
wWinDialogCreate(wControl_p parent,
    const char* helpStr,
    const char* titleStr,
    const char* nameStr,
    long option,
    wWinCallBack_p winProc,
    void* data)
{
    GtkWidget* dialog;
    GtkBuilder* builder;
    GtkWidget* parentWindow;
    gchar* resourcePath;
    char* tempStr = NULL;
    struct window* dcontrol;

    wControl_p winDialog = wlibControlNew(W_DIALOG, parent, helpStr, data);
    dcontrol = WLIB_GET_DATA_PTR(winDialog, window);

    if (option & DO_FILESYSTEM) {
        // in case filename is given, load builder and create a name from 
        // the base filename without extension
        resourcePath = g_strdup(nameStr);
        builder = gtk_builder_new_from_file(resourcePath);
        tempStr = g_path_get_basename(resourcePath);
        tempStr[strlen(tempStr) - 3] = '\0';
        nameStr = tempStr;
    }
    else {
        resourcePath = g_strconcat(XTRKCAD_RESOURCE_PATH,
            nameStr,
            ".ui",
            NULL);
        builder = gtk_builder_new_from_resource(resourcePath);
    }

    dialog = GTK_WIDGET(gtk_builder_get_object(builder, nameStr));
    g_free(resourcePath);
    resourcePath = NULL;

    if (!dialog)
    {
        GString* errorMessage = g_string_new("Dialog id is not: ");
        g_string_append_printf(errorMessage, "%s", nameStr);
        wNoticeEx(NT_ERROR,
            errorMessage->str,
            "OK",
            NULL);
        g_string_free(errorMessage, TRUE);
        g_free(tempStr);
        return(NULL);
    }


    if (parent == NULL) {
        parentWindow = wlibAppWinGetMain();
    }
    else {
        parentWindow = parent->widget;
    }

    RestoreWindow(dialog, nameStr);

    if (option & F_CENTER) {
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }

    g_signal_connect(G_OBJECT(dialog), "configure_event",
        G_CALLBACK(dialog_configure_event), winDialog);

    g_signal_connect(G_OBJECT(dialog), "response",
        G_CALLBACK(response_signal), winDialog);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parentWindow));
    gtk_window_set_title(GTK_WINDOW(dialog), titleStr);

    gtk_widget_show(dialog);
    dcontrol->option = option & BO_USEBUILDER;
    winDialog->widget = dialog;
    winDialog->name = g_strdup(nameStr);
    dcontrol->winProc = winProc;
    dcontrol->builder = builder;
    dcontrol->option = option & BO_USEBUILDER;

    g_free(tempStr);
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
 * \param data      IN User context information
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
    void* data)
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
    //    winProc, data);
    return win;
}

