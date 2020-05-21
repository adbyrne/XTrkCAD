/** \file help.c
 * main help function
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis and (C) 2007 Martin Fischer
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

#include "misc.h"

#include "gtkint.h"
#include "i18n.h"


/**
 * Handle the commands issued from the Help drop-down. Currently, we only have a table
 * of contents, but search etc. might be added in the future.
 *
 * \param data IN command value
 *
 */

static void
DoHelpMenu(void *data)
{
    int func = (intptr_t)data;

    const char * topic;

    switch (func) {
    case 1:
        wHelp("index");
        break;

    case 3:
    	topic = GetCurCommandName();
    	if (topic && topic[0])
    		wHelp(topic);
    	break;

    default:
        break;
    }

    return;
}

void wDoAccelHelp(wAccelKey_e key, void * context) {
	DoHelpMenu(context);
}

/**
 * Add the entries for Help to the drop-down.
 *
 * \param m IN handle of drop-down
 *
 */

void wMenuAddHelp(wMenu_p m)
{
    wMenuPushCreate(m, NULL, _("&Contents"), 0, DoHelpMenu, (void*)1);
    wMenuPushCreate(m, NULL, _("Co&mmand Context help"), 0, DoHelpMenu, (void*)3);
}
