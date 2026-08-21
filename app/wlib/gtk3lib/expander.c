/** \file expander.c
 * code for the expander
 */

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


#include "gtkint.h"

static void
expanderExpandedNotify(GObject *obj, GParamSpec *pspec, gpointer user)
{
	wControl_p b = (wControl_p)user;
	struct expander *e = CONTROL_GET_ATTRIBUTES_PTR(b, expander);
	gboolean expanded = gtk_expander_get_expanded(GTK_EXPANDER(obj));

	if (e->inProgrammaticSet) {
		return;                       /* program-initiated: do NOT fire */
	}
	if (e->action) {
		e->action(b, b->name, expanded ? TRUE : FALSE, b->context);
	}
}

wControl_p
wExpanderCreate(wWin_p parent, const char *id, wControl_p win, void *context)
{
	GtkWidget *widget = wlibWidgetFromIdWarn(win, id);
	wControl_p b;
	struct expander *e;

	if (!widget || !GTK_IS_EXPANDER(widget)) { return NULL; }

	b = wlibControlNew(B_EXPANDER, parent, id, context);
	b->widget = widget;

	e = CONTROL_GET_ATTRIBUTES_PTR(b, expander);
	e->notifyHandler = g_signal_connect(widget, "notify::expanded",
	                                    G_CALLBACK(expanderExpandedNotify), b);
	return b;
}

void
wExpanderShow(wControl_p b, wBool_t reveal)
{
	struct expander *e;
	if (!b || b->type != B_EXPANDER) { return; }
	e = CONTROL_GET_ATTRIBUTES_PTR(b, expander);

	e->inProgrammaticSet++;
	gtk_expander_set_expanded(GTK_EXPANDER(b->widget), reveal ? TRUE : FALSE);
	e->inProgrammaticSet--;
}

void
wExpanderSetToggleCallback(wControl_p b, wExpanderToggleCallback_p action)
{
	struct expander *e;
	if (!b || b->type != B_EXPANDER) { return; }
	e = CONTROL_GET_ATTRIBUTES_PTR(b, expander);
	e->action = action;
}

void
wExpanderSetSummary(wControl_p b, const char *summary)
{
	GtkWidget *label;
	if (!b || b->type != B_EXPANDER) { return; }
	label = gtk_expander_get_label_widget(GTK_EXPANDER(b->widget));
	if (GTK_IS_LABEL(label)) {
		gtk_label_set_text(GTK_LABEL(label), summary ? summary : "");
	} else {
		gtk_expander_set_label(GTK_EXPANDER(b->widget), summary ? summary : "");
	}
}