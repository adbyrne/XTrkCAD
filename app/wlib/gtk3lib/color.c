/** \file color.c
 * code for the color selection dialog and color button
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

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

wDrawColor wDrawColorWhite;
wDrawColor wDrawColorBlack;

#define RGB(R,G,B) ( ((long)((R)&0xFF)<<16) | ((long)((G)&0xFF)<<8) | ((long)((B)&0xFF)) )
#define RGBA(RGB,A) ( ((long)((RGB)&0xFFFFFF)) | ((long)((A)&0xFF)<<24) )

/**
 * Get a gray color
 *
 * \param percent IN gray value required
 * \return definition for gray color
 */

wDrawColor wDrawColorGray(
        int percent)
{
	if (percent <= 0) {
		return wDrawColorBlack;
	} else if (percent > 100) {
		return wDrawColorWhite;
	}

	return RGB((percent*256/100), percent*256/100, percent*256/100);
}


/**
 * \todo eliminate as we're not using a palete anymore
 *
 * \param rgb0 IN desired color
 * \return palette index of matching color
 */

wDrawColor wDrawFindColor(
        long rgb0)
{
	return rgb0;
}

/**
 * \todo eliminate as we're not using a palete anymore
 * Get the RGB code for a palette entry
 *
 * \param color IN the palette index
 * \return RGB code
 */

long wDrawGetRGB(
        wDrawColor color)
{
	return color;
}

/**
 * Get the color definition from the "index"
 *
 * \param color IN index into palette
 * \param normal IN normal or inverted color
 * \return  the selected color definition
 * 
 * \todo Check usage 
 */

GdkRGBA wlibGetColor(
        wDrawColor color,
        wBool_t normal)
{
	GdkRGBA out;
	out.red = ((color&0x00FF0000)>>16)/256.0;
	out.green = ((color&0x0000FF00)>>8)/256.0;
	out.blue = ((color&0x000000FF))/256.0;
	if ((color&0xFF000000) == 0) { out.alpha = 1.0; }
	else { out.alpha = ((color&0xFF000000)>>24)/256.0; }

	if (normal) {
		return out;
	} else {
		out.red = 1.0-out.red;
		out.green = 1.0-out.green;
		out.blue = 1.0-out.blue;
		return out;
	}
}


/*
 *****************************************************************************
 *
 * Color Selection Button
 *
 *****************************************************************************
 */
/**
 * Get the selected color from the color button.
 * 
 * \param widget	color button
 * \return			selected color
 */

wDrawColor
wlibColorButtonGetColor(GtkColorButton* widget)
{
	GdkRGBA rgba;
	wDrawColor rgb;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &rgba);

	rgb = ((long)(rgba. red * 255.0) << 16) + 
		((long)(rgba.green * 255.0) << 8) + 
		((long)(rgba.blue * 255.0));

	return(rgb);
}
/**
 * Handle the color-set signal.
 *
 * \param widget  color button
 * \param user_data
 */

static void
colorChange(GtkColorButton *widget, wColorButton_p user_data)
{
	long rgb;

	rgb = wlibColorButtonGetColor(widget);

	if (user_data->valueP) {
		*(user_data->valueP) = rgb;
	}

	if (user_data->action) {
		user_data->action(user_data->data, rgb);
	}
}

/**
 * Set the color for a color button
 *
 * \param bb IN button
 * \param color IN palette index for color to use
 * \return    describe the return value
 */

void wColorSelectButtonSetColor(
        wColorButton_p bb,
        wDrawColor color)
{
	GdkRGBA rgba;

	rgba.red = ((color&0x00FF0000)>>16)/256.0;
	rgba.green = ((color&0x0000FF00)>>8)/256.0;
	rgba.blue = (color&0x000000FF)/256.0;
	rgba.alpha = 1.0;

	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(bb->widget),
	                           &rgba);

}


/**
 * Get the current color
 *
 * \param bb IN color button handle
 * \return  color in RGBA format
 */

wDrawColor wColorSelectButtonGetColor(
        wColorButton_p bb)
{
	wDrawColor rgb = wlibColorButtonGetColor(GTK_COLOR_BUTTON(bb->widget));

	return (rgb);
}

/**
 * Create the button showing the current paint color and starting the color 
 * selection dialog.
 * 
 * ### Usage in dialogs
 *
 * - Generated: yes
 *
 * ### Options
 * BB_DEFAULT
 * : set button as default for dialog
 *
 * \param IN parent parent window
 * \param IN x, y		x, y position in grid 
 * \param IN helpStr	tooltip help string
 * \param IN labelStr	title for color selection dialog
 * \param IN option
 * \param IN width
 * \param IN valueP		current color
 * \param IN action		button callback procedure
 * \param IN data		user data to pass to callback procedure
 * 
 * \return bb handle for created button
 * 
 * \todo Color button in builder definition 
 * 
 */

wColorButton_p wColorSelectButtonCreate(
        wWindow_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char 	* helpStr,
        const char	* labelStr,
        long 	option,
        wWinPix_t 	width,
        wDrawColor *valueP,
        wColorSelectButtonCallBack_p action,
        void 	* data)
{
	wColorButton_p b;

	b = g_malloc0(sizeof(struct wColorButton_t));

	if (option & BO_USETEMPLATE) {
		/** 

		*/
	} else {
		GtkGrid* grid = GTK_GRID(wlibWidgetFromIdWarn(parent, "layoutgrid"));

		b->widget = gtk_color_button_new();
		if (!b->widget) { exit(4); }

		g_signal_connect(b->widget, "color-set",
			G_CALLBACK(colorChange), b);

		if (option & BB_DEFAULT) {
			gtk_widget_set_can_default(b->widget, TRUE);
			gtk_widget_grab_default(b->widget);
			gtk_window_set_default(GTK_WINDOW(parent->gtkWindow), b->widget);
		}

		if (labelStr) {
			gtk_color_button_set_title(GTK_COLOR_BUTTON(b->widget), labelStr);
		}

		gtk_widget_set_size_request(b->widget, 20, 20);
		gtk_widget_set_hexpand(b->widget, FALSE);
		gtk_widget_set_vexpand(b->widget, FALSE);

		gtk_grid_attach(grid, b->widget, x, y, width, 1);
		gtk_widget_show(b->widget);
	}

	b->action = action;
	b->data = data;

	wColorSelectButtonSetColor(b, (valueP?*valueP:0));

	return b;
}
