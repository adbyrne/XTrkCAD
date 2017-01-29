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

#include <stdio.h>
#include <assert.h>

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
    GdkPixbuf * pixbuf;
    char ** pixmapData;
    char * oldline1;
    static char newline1[] = " \tc None s None";
    char line0[40];
    char line2[40];
    int row,col,wb;
    long rgb;
    const char * bits;
    GdkColor *transparent;
    assert(ip != NULL);
    transparent = &gtk_widget_get_style(gtkMainW->gtkwin)->bg[gtk_widget_get_state(
                      gtkMainW->gtkwin)];

    if (ip->gtkIconType == gtkIcon_pixmap) {
        pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)ip->bits);
    } else {
        wb = (ip->w+7)/8;
        pixmapData = (char**)g_malloc((3+ip->h) * sizeof *pixmapData);
        pixmapData[0] = line0;
        rgb = wDrawGetRGB(ip->color);
        sprintf(line0, " %d %d 2 1", ip->w, ip->h);
        sprintf(line2, "# c #%2.2lx%2.2lx%2.2lx", (rgb>>16)&0xFF, (rgb>>8)&0xFF,
                rgb&0xFF);
        pixmapData[1] = ". c None s None";
        pixmapData[2] = line2;
        bits = ip->bits;

        for (row = 0; row<ip->h; row++) {
            pixmapData[row+3] = (char*)g_malloc((ip->w+1) * sizeof **pixmapData);

            for (col = 0; col<ip->w; col++) {
                if (bits[ row*wb+(col>>3) ] & (1<<(col&07))) {
                    pixmapData[row+3][col] = '#';
                } else {
                    pixmapData[row+3][col] = '.';
                }
            }

            pixmapData[row+3][ip->w] = 0;
        }

        pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)pixmapData);

        for (row = 0; row<ip->h; row++) {
            g_free(pixmapData[row+3]);
        }
    }

    return pixbuf;
}


