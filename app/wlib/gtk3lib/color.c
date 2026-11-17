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
#include <math.h>

#include "gtkint.h"

wDrawColor wDrawColorWhite = 0xFFFFFF;
wDrawColor wDrawColorBlack = 0x000000;

#define RGB(R,G,B) ( ((long)((R)&0xFF)<<16) | ((long)((G)&0xFF)<<8) | ((long)((B)&0xFF)) )

/**
 * Get a gray color
 *
 * \param percent IN brightness, 0 = black, 100 = white
 * \return definition for gray color
 */

wDrawColor wDrawColorGray(
        int percent)
{
	int level;

	if (percent <= 0) {
		return wDrawColorBlack;
	} else if (percent >= 100) {
		return wDrawColorWhite;
	}

	level = (percent * 255 + 50) / 100;
	return RGB(level, level, level);
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

	rgb = wRGB(lround(rgba.red * 255.0),
	           lround(rgba.green * 255.0),
	           lround(rgba.blue * 255.0));

	return(rgb);
}
/**
 * Handle the color-set signal.
 *
 * \param widget  color button
 * \param user_data
 */

static void
colorChange(GtkColorButton *widget, wControl_p user_data)
{
	unsigned long rgb;

	rgb = wlibColorButtonGetColor(widget);

	if (user_data->attributes.colorbutton.valueP) {
		*(user_data->attributes.colorbutton.valueP) = rgb;
	}

	if (user_data->attributes.colorbutton.action) {
		user_data->attributes.colorbutton.action(user_data->context, rgb);
	}
}

/**
 * Set the color for a color button
 *
 * \param bb IN button
 * \param color IN color to use
 */

void wColorSelectButtonSetColor(
        wControl_p bb,
        wDrawColor color)
{
	GdkRGBA rgba = wlibGetColor(color);

	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(bb->widget),
	                           &rgba);

}


/**
 * Get the current color
 *
 * \param bb IN color button handle
 * \return  color as 0xRRGGBB
 */

wDrawColor wColorSelectButtonGetColor(
        wControl_p bb)
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
 * - runtime: yes
 * - builder: yes
 *
 * ### Options
 * BB_DEFAULT
 * : set button as default for dialog
 *
 * \param[in] parent parent window
 * \param[in] x, y		x, y position in grid
 * \param[in] helpStr	tooltip help string
 * \param[in] labelStr	title for color selection dialog
 * \param[in] option
 * \param[in] width
 * \param[in] valueP		current color
 * \param[in] action		button callback procedure
 * \param[in] context		user context to pass to callback procedure
 *
 * \return bb handle for created button
 *
 */

wControl_p wColorSelectButtonCreate(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char 	* helpStr,
        const char	* labelStr,
        long 	option,
        wWinPix_t 	width,
        wDrawColor *valueP,
        wColorSelectButtonCallBack_p action,
        void 	* context)
{
	wControl_p b;
	struct colorbutton* ccontrol;

	b = wlibControlNew(B_COLORBUTTON,parent, helpStr, context);
	ccontrol = CONTROL_GET_ATTRIBUTES_PTR(b, colorbutton);
	ccontrol->action = action;
	ccontrol->valueP = valueP;

	if (ISDEFINEDINBUILDER(parent)) {
		b->widget = wlibWidgetFromIdWarn(parent, helpStr);
	} else {
		b->widget = gtk_color_button_new();
		if (!b->widget) { exit(1); }


		if (option & BB_DEFAULT) {
			gtk_widget_set_can_default(b->widget, TRUE);
			gtk_widget_grab_default(b->widget);
			gtk_window_set_default(GTK_WINDOW(parent->widget), b->widget);
		}

		if (labelStr) {
			gtk_color_button_set_title(GTK_COLOR_BUTTON(b->widget), labelStr);
			wlibAddLabel(b, x - 1, y, labelStr);
		}

		gtk_widget_set_size_request(b->widget, 20, 20);
		gtk_widget_set_hexpand(b->widget, FALSE);
		gtk_widget_set_vexpand(b->widget, FALSE);

		wlibBasicGridAttach(parent, b->widget, x, y, width, 1);
		gtk_widget_show(b->widget);
	}

	g_signal_connect(b->widget, "color-set",
	                 G_CALLBACK(colorChange), b);

	wColorSelectButtonSetColor(b, (valueP?*valueP:0));

	return b;
}
