/** \file pixbuf.c
 * Create pixbuf from various bitmap formats
 */

/*
 *
 * Copyright 2016 Martin Fischer <m_fischer@users.sf.net>
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

/**
 * Create a pixbuf from a wIcon
 *
 * \param ip IN widget
 * \returns a valid pixbuf
 */

GdkPixbuf* wlibMakePixbuf(
    wIcon_p ip)
{
    g_assert(ip != NULL);

    if (ip->gtkIconType == ICON_PIXBUF)
        return((GdkPixbuf*)ip->bits);
    else
        return(NULL);

}


