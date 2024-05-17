/** \file notice.c
* Misc. message type windows
*
* Copyright 2016 Martin Fischer <m_fischer@sf.net>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
* MA 02110-1301, USA.
*
*
*/

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

/**
 * Show a notification window with a yes/no reply and an icon.
 *
 * \param type IN type of message: Information, Warning, Error
 * \param msg  IN message to display
 * \param yes  IN text for accept button
 * \param no   IN text for cancel button
 * \return    True when accept was selected, false otherwise
 */

int wNoticeEx(int type,
              const char * msg,
              const char * yes,
              const char * no)
{

	int res;
	unsigned flag;
	char *headline;
	GtkWidget *dialog;
	GtkWindow *parent = NULL;

	switch (type) {
	case NT_INFORMATION:
		flag = GTK_MESSAGE_INFO;
		headline = _("Information");
		break;

	case NT_WARNING:
		flag = GTK_MESSAGE_WARNING;
		headline = _("Warning");
		break;

	case NT_ERROR:
		flag = GTK_MESSAGE_ERROR;
		headline = _("Error");
		break;
	}

	if (gtkMainW) {
		parent = GTK_WINDOW(gtkMainW->gtkwin);
	}

	wDestroySplash();

	dialog = gtk_message_dialog_new(parent,
	                                GTK_DIALOG_DESTROY_WITH_PARENT,
	                                flag,
	                                ((no==NULL)?GTK_BUTTONS_OK:GTK_BUTTONS_YES_NO),
	                                "%s", msg);
	gtk_window_set_title(GTK_WINDOW(dialog), headline);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return res == GTK_RESPONSE_OK  || res == GTK_RESPONSE_YES;
}


/**
 * Popup up a notice box with one or two buttons.
 * When this notice box is displayed the application is paused and
 * will not response to other actions.
 *
 * @param msg IN message
 * @param yes IN first button label
 * @param no IN second button label (or NULL)
 * @returns TRUE for first FALSE for second button
 */

int wNotice(
        const char * msg,		/* Message */
        const char * yes,		/* First button label */
        const char * no)		/* Second label (or 'NULL') */
{
	return wNotice3(msg, yes, no, NULL);
}

/** \brief Popup a notice box with three buttons.
 *
 * Popup up a notice box with three buttons. The default is set to the
 * right-most button. So this should trigger a function that does not
 * cause any dataloss or other damage.
 * When this notice box is displayed the application is paused and
 * will not response to other actions.
 *
 * Pushing the first button returns 1
 * Pushing the second button returns 0
 * Pushing the third button returns -1
 *
 * \param msg Text to display in message box
 * \param yes First button label
 * \param no  Second label (or 'NULL')
 * \param cancel Third button label (or 'NULL')
 *
 * \returns 1, 0 or -1
 */

int wNotice3(
        const char * msg,		/* Message */
        const char * affirmative,		/* First button label */
        const char * cancel,		/* Second label (or 'NULL') */
        const char * alternate)
{
	int noticeValue;
	GtkDialog* dialog;
	GtkWidget* content_area;
	GtkBox* box;
	GtkLabel* label;
	GtkImage* image;

	wDestroySplash();

	dialog = GTK_DIALOG(gtk_dialog_new());
	gtk_window_set_title(GTK_WINDOW(dialog), "Notice");
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 18);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	content_area = gtk_dialog_get_content_area(dialog);
	box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12));
	gtk_widget_set_margin_bottom(GTK_WIDGET(box), 12);

	image = GTK_IMAGE(gtk_image_new_from_icon_name("dialog-warning",
	                  GTK_ICON_SIZE_DIALOG));
	gtk_box_pack_start(box, GTK_WIDGET(image), 0, 0, 0);

	label = GTK_LABEL(gtk_label_new(msg));
	gtk_label_set_line_wrap(label, TRUE);
	gtk_label_set_selectable(label, TRUE);
	gtk_box_pack_start(box, GTK_WIDGET(label), 0,0,0);

	gtk_container_add(GTK_CONTAINER(content_area), GTK_WIDGET(box));
	gtk_widget_show_all(GTK_WIDGET(box));

	gtk_dialog_add_button(dialog, affirmative, 1);

	if (cancel) {
		gtk_dialog_add_button(dialog, cancel, 0);
		if (alternate) {
			gtk_dialog_add_button(dialog, alternate, -1);
		}
	}

	gtk_widget_show(GTK_WIDGET(dialog));

	noticeValue = gtk_dialog_run(dialog);

	gtk_widget_destroy(GTK_WIDGET(dialog));
	return noticeValue;
}



