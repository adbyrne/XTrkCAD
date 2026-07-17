/**
 * \file   tag.c
 * \brief  Combined widget existing of a label and a button
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

/**
 *
 */

void
wTagSetLabel( wControl_p tagControl, const char * text)
{
	struct tag *privateData;
	if(tagControl->widget == NULL) {
		abort();
	}

	privateData = CONTROL_GET_ATTRIBUTES_PTR(tagControl, tag);
	gtk_label_set_text(privateData->label, wlibConvertInput(text));
}

const char *
wTagGetLabel(wControl_p tagControl)
{
	struct tag *privateData;
	if(tagControl->widget == NULL) {
		abort();
	}
	privateData = CONTROL_GET_ATTRIBUTES_PTR(tagControl, tag);
	return gtk_label_get_text(privateData->label);
}

static void
TagButtonClicked(GtkWidget *widget, void *userdata)
{
	struct tag* thisTag = CONTROL_GET_ATTRIBUTES_PTR(((wControl_p)userdata), tag);

	if(thisTag->callback) {
		(thisTag->callback)(((wControl_p)userdata)->context);
	}
}

/**
 * Create a tag. A tag is a combined widget consisting of a box, a label and a button.
 *
 * ### Usage in dialogs
 *
 * - Runtime: no
 * - Builder: yes
 *
 * ### Options
 * None
 *
 * \param parent IN parent window
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param action IN Callback
 * \param context IN User context
 * \returns tag widget
 */

wControl_p
wTagCreate(
        wControl_p	parent,
        const char* helpStr,
        const char* labelStr,
        wButtonCallBack_p action,
        void* context)
{
	wControl_p newTag = wlibControlNew(B_TAG, parent, helpStr, context);
	struct tag* privateTag = CONTROL_GET_ATTRIBUTES_PTR(newTag, tag);
	GList* children;
	GList* current;

	newTag->widget = wlibWidgetFromIdWarn(parent, helpStr);
	privateTag->callback = action;

	newTag->widget = wlibWidgetFromIdWarn(parent, helpStr);

	children = gtk_container_get_children(GTK_CONTAINER(newTag->widget));
	for (current = children; current != NULL; current = g_list_next(current)) {
		if(GTK_IS_LABEL(current->data)) {
			privateTag->label = current->data;
			continue;
		}
		if(GTK_IS_BUTTON(current->data)) {
			privateTag->button = current->data;
			g_signal_connect(privateTag->button, "clicked", TagButtonClicked, newTag);
			continue;
		}

		printf("Child of tag %s is ignored!\n", helpStr);
	}
	g_list_free(children);

	if(labelStr && privateTag->label) {
		wTagSetLabel(newTag, labelStr);
	}

	return newTag;
}
