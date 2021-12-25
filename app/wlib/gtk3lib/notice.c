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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

static char * wlibChgMnemonic(char *label);

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
 * Popup up a notice box with three buttons.
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
        const char * msg,			/* Message */
        const char * affirmative,	/* First button label */
        const char * cancel,		/* Second label (or 'NULL') */
        const char * alternate)
{
	GtkWidget *nw;
	int resultCode;

	char *aff = wlibChgMnemonic((char *) affirmative);
	char *can = wlibChgMnemonic((char *) cancel);
	char *alt = wlibChgMnemonic((char *) alternate);

	wDestroySplash();

	nw = gtk_message_dialog_new (GTK_WINDOW_TOPLEVEL,
	                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	                             GTK_MESSAGE_WARNING,
	                             GTK_BUTTONS_NONE,
	                             msg );

	if(alt) {
		gtk_dialog_add_button( GTK_DIALOG(nw),
		                       alt,
		                       GTK_RESPONSE_CANCEL);
	}

	if( can ) {
		gtk_dialog_add_button( GTK_DIALOG(nw),
		                       can,
		                       GTK_RESPONSE_NO );
	}

	gtk_dialog_add_button( GTK_DIALOG(nw),
	                       aff,
	                       GTK_RESPONSE_YES );
	gtk_dialog_set_default_response (GTK_DIALOG(nw),
	                                 GTK_RESPONSE_YES);

	int result = gtk_dialog_run (GTK_DIALOG (nw));
	gtk_widget_destroy (GTK_WIDGET (nw));

	switch(result) {
	case GTK_RESPONSE_YES:
		resultCode = 1;
		break;
	case GTK_RESPONSE_NO:
		resultCode = 0;
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		resultCode = -1;
		break;
	}

	if (aff) {
		free(aff);
	}

	if (can) {
		free(can);
	}

	if (alt) {
		free(alt);
	}

	return resultCode;
}

/* \brief Convert label string from Windows mnemonic to GTK
 *
 * The first occurence of '&' in the passed string is changed to '_'
 *
 * \param label the string to convert
 * \return pointer to modified string, has to be free'd after usage
 *
 */
static
char * wlibChgMnemonic(char *label)
{
	char *ptr;
	char *cp;

	if(!label) {
		return NULL;
	}

	cp = strdup(label);

	ptr = strchr(cp, '&');

	if (ptr) {
		*ptr = '_';
	}

	return (cp);
}


