/**
 * \file   popupmenu.c
 * \brief  Context menu
 *
 * \author Martin Fischer
 */

 /*  XTrackCad - Model Railroad CAD
  *  Copyright (C) 2005, 2024 Dave Bullis
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
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

#include <wlib.h>


static gint popup_char_event(
	GtkWidget* widget,
	GdkEventKey* event,
	wControl_p popupControl)
{
	UpdateModifierKeyState(event);

	return TRUE;
}

/**
 * Create a popup menu (context menu)
 *
 * \param w 		IN parent window
 * \param labelStr 	IN label
 * \return    the created menu
 */

wControl_p wMenuPopupCreate(
	wControl_p parent,
	const char* labelStr)
{
	wControl_p popupControl = NULL;
	struct menu* popupAttributes;

	popupControl = wlibControlNew(B_MENU, parent, NULL, NULL);
	popupAttributes = CONTROL_GET_ATTRIBUTES_PTR(popupControl, menu);

	popupControl->widget = gtk_menu_new();

	g_signal_connect( G_OBJECT (popupControl->widget), "key_press_event",
	 		G_CALLBACK(popup_char_event), popupControl);
	g_signal_connect( G_OBJECT (popupControl->widget), "key_release_event",
	 		G_CALLBACK (popup_char_event), popupControl);
	gtk_widget_set_events ( GTK_WIDGET(popupControl->widget), 
							GDK_EXPOSURE_MASK|GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK );

	return popupControl;
}

 void wMenuPopupShow( wControl_p mp )
 {
     gtk_menu_popup_at_pointer( GTK_MENU(mp->widget), NULL );
 }

