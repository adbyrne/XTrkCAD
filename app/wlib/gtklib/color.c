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
#include <assert.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

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

#define MAX_COLOR_DISTANCE (3)

static GArray *colorMap_garray = NULL; // Change to use glib array

static colorMap_t colorMap[] = {
    { 255, 255, 255 },	/* White */
    {   0,   0,   0 },	/* Black */
    { 255,   0,   0 },	/* Red */
    {   0, 255,   0 },	/* Green */
    {   0,   0, 255 },	/* Blue */
    { 255, 255,   0 },	/* Yellow */
    { 255,   0, 255 },	/* Purple */
    {   0, 255, 255 },	/* Aqua */
    { 128,   0,   0 },	/* Dk. Red */
    {   0, 128,   0 },	/* Dk. Green */
    {   0,   0, 128 },	/* Dk. Blue */
    { 128, 128,   0 },	/* Dk. Yellow */
    { 128,   0, 128 },	/* Dk. Purple */
    {   0, 128, 128 },	/* Dk. Aqua */
    {  65, 105, 225 },      /* Royal Blue */
    {   0, 191, 255 },	/* DeepSkyBlue */
    { 125, 206, 250 },	/* LightSkyBlue */
    {  70, 130, 180 },	/* Steel Blue */
    { 176, 224, 230 },	/* Powder Blue */
    { 127, 255, 212 },	/* Aquamarine */
    {  46, 139,  87 },	/* SeaGreen */
    { 152, 251, 152 },	/* PaleGreen */
    { 124, 252,   0 },	/* LawnGreen */
    {  50, 205,  50 },	/* LimeGreen */
    {  34, 139,  34 },	/* ForestGreen */
    { 255, 215,   0 },	/* Gold */
    { 188, 143, 143 },	/* RosyBrown */
    { 139, 69, 19 },	/* SaddleBrown */
    { 245, 245, 220 },	/* Beige */
    { 210, 180, 140 },	/* Tan */
    { 210, 105, 30 },	/* Chocolate */
    { 165, 42, 42 },	/* Brown */
    { 255, 165, 0 },	/* Orange */
    { 255, 127, 80 },	/* Coral */
    { 255, 99, 71 },	/* Tomato */
    { 255, 105, 180 },	/* HotPink */
    { 255, 192, 203 },	/* Pink */
    { 176, 48, 96 },	/* Maroon */
    { 238, 130, 238 },	/* Violet */
    { 160, 32, 240 },	/* Purple */
    {  16,  16,  16 },	/* Gray */
    {  32,  32,  32 },	/* Gray */
    {  48,  48,  48 },	/* Gray */
    {  64,  64,  64 },	/* Gray */
    {  80,  80,  80 },	/* Gray */
    {  96,  96,  96 },	/* Gray */
    { 112, 112, 122 },	/* Gray */
    { 128, 128, 128 },	/* Gray */
    { 144, 144, 144 },	/* Gray */
    { 160, 160, 160 },	/* Gray */
    { 176, 176, 176 },	/* Gray */
    { 192, 192, 192 },	/* Gray */
    { 208, 208, 208 },	/* Gray */
    { 224, 224, 224 },	/* Gray */
    { 240, 240, 240 },	/* Gray */
    {   0,   0,   0 }	/* BlackPixel */
};

#define NUM_GRAYS (16)

static GdkColormap * gtkColorMap;

static char lastColorChar = '!';

/**
 * Get a gray color
 *
 * \param percent IN gray value required
 * \return definition for gray color
 */

wDrawColor wDrawColorGray(
    int percent)
{
    int n;
    long rgb;
    n = (percent * (NUM_GRAYS+1)) / 100;

    if (n <= 0) {
        return wDrawColorBlack;
    } else if (n > NUM_GRAYS) {
        return wDrawColorWhite;
    } else {
        n = (n*256)/NUM_GRAYS;
        rgb = RGB(n, n, n);
        return wDrawFindColor(rgb);
    }
}

/**
 * Get the color map for the main window
 *
 * \return
 */

void wlibGetColorMap(void)
{
    if (gtkColorMap) {
        return;
    }

    gtkColorMap = gtk_widget_get_colormap(gtkMainW->widget);
    return;
}

/**
 * Initialize a colorMap entry
 * \todo no idea what this is required for
 *
 * \param t IN color code
 * \return
 */

static void init_colorMapValue(colorMap_t * t)
{
    t->rgb = RGB(t->red, t->green, t->blue);
    t->normalColor.red = t->red*65535/255;
    t->normalColor.green = t->green*65535/255;
    t->normalColor.blue = t->blue*65535/255;
    gdk_colormap_alloc_color(gtkColorMap, &t->normalColor, FALSE, TRUE);
    t->invertColor = t->normalColor;
    t->invertColor.pixel ^= g_array_index(colorMap_garray, colorMap_t,
                                          wDrawColorWhite).normalColor.pixel;
    t->colorChar = lastColorChar++;

    if (lastColorChar >= 0x7F) {
        lastColorChar = '!'+1;
    } else if (lastColorChar == '"') {
        lastColorChar++;
    }
}

/**
 * Allocate a color map and initialize with application default colors
 *
 * \return
 */

static void init_colorMap(void)
{
    int gint;
    colorMap_garray = g_array_sized_new(TRUE, TRUE, sizeof(colorMap_t),
                                        sizeof(colorMap)/sizeof(colorMap_t));
    g_array_append_vals(colorMap_garray, &colorMap,
                        sizeof(colorMap)/sizeof(colorMap_t));

    for (gint=0; gint<colorMap_garray->len; gint++) {
        init_colorMapValue(&g_array_index(colorMap_garray, colorMap_t, gint));
    }
}

/**
 * Find the closest color from the palette and add a new color if there
 * is no close match
 * \todo improve method for finding best match (squared distances)
 *
 * \param rgb0 IN desired color
 * \return palette index of matching color
 */

wDrawColor wDrawFindColor(
    long rgb0)
{
    wDrawColor cc;
    int r0, g0, b0;
    int d0;
    int i;
    colorMap_t tempMapValue;
    wlibGetColorMap();
    cc = wDrawColorBlack;
    r0 = (int)(rgb0>>16)&0xFF;
    g0 = (int)(rgb0>>8)&0xFF;
    b0 = (int)(rgb0)&0xFF;
    d0 = 256*3;

    // Initialize garray if needed
    if (colorMap_garray == NULL) {
        init_colorMap();
    }

    // Iterate over entire garray
    for (i=0; i<colorMap_garray->len; i++) {
        int d1;
        colorMap_t * cm_p;

        cm_p = &g_array_index(colorMap_garray, colorMap_t, i);
        d1 = abs(r0-cm_p->red) + abs(g0-cm_p->green) + abs(b0-cm_p->blue);

        if (d1 == 0) {
            return i;
        }

        if (d1 < d0) {
            d0 = d1;
            cc = i;
        }
    }

    if (d0 <= MAX_COLOR_DISTANCE) {
        return cc;
    }

    // No good value - so add one
    tempMapValue.red = r0;
    tempMapValue.green = g0;
    tempMapValue.blue = b0;
    init_colorMapValue(&tempMapValue);
    g_array_append_val(colorMap_garray,tempMapValue);
    return i;
}

/**
 * Get the RGB code for a palette entry
 *
 * \param color IN the palette index
 * \return RGB code
 */

long wDrawGetRGB(
    wDrawColor color)
{
    colorMap_t * colorMap_e;
    wlibGetColorMap();

    if (colorMap_garray == NULL) {
        init_colorMap();
    }

    if (color < 0 || color > colorMap_garray->len) {
        abort();
    }

    colorMap_e = &g_array_index(colorMap_garray, colorMap_t, color);
    return colorMap_e->rgb;
}

/**
 * Get the color definition from the palette index
 *
 * \param color IN index into palette
 * \param normal IN normal or inverted color
 * \return  the selected color definition
 */

GdkColor* wlibGetColor(
    wDrawColor color,
    wBool_t normal)
{
    colorMap_t * colorMap_e;
    wlibGetColorMap();

    if (colorMap_garray == NULL) {
        init_colorMap();
    }

    if (color < 0 || color > colorMap_garray->len) {
        abort();
    }

    colorMap_e = &g_array_index(colorMap_garray, colorMap_t, color);

    if (normal) {
        return &colorMap_e->normalColor;
    } else {
        return &colorMap_e->invertColor;
    }
}


/*
 *****************************************************************************
 *
 * Color Selection Button
 *
 *****************************************************************************
 */

typedef struct {
    wDrawColor * valueP;
    const char * labelStr;
    wColorSelectButtonCallBack_p action;
    void * data;
    wDrawColor color;
    wButton_p button;
} colorData_t;

/**
 * Handle the color-set signal.
 *
 * \param widget  color button
 * \param user_data
 */

static void
colorChange(GtkColorButton *widget, gpointer user_data)
{
    colorData_t *cd = user_data;
    GdkColor newcolor;
    long rgb;

    gtk_color_button_get_color(widget, &newcolor);

    rgb = RGB((int)(newcolor.red/256), (int)(newcolor.green/256),
              (int)(newcolor.blue/256));
    cd->color = wDrawFindColor(rgb);

    if (cd->valueP) {
        *(cd->valueP) = cd->color;
    }

    if (cd->action) {
        cd->action(cd->data, cd->color);
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
    wButton_p bb,
    wDrawColor color)
{
    GdkColor *colorOfButton = wlibGetColor(color, TRUE);

    gtk_color_button_set_color(GTK_COLOR_BUTTON(bb->widget),
                               colorOfButton);
    ((colorData_t*)((wControl_p)bb)->data)->color = color;
}


/**
 * Get the current palette index for a color button
 *
 * \param bb IN button handle
 * \return  palette index
 */

wDrawColor wColorSelectButtonGetColor(
    wButton_p bb)
{
    return ((colorData_t*)((wControl_p)bb)->data)->color;
}

/**
 * Create the button showing the current paint color and starting the color selection dialog.
 *
 * \param IN parent parent window
 * \param IN x x coordinate
 * \param IN Y y coordinate
 * \param IN helpStr balloon help string
 * \param IN labelStr Button label ???
 * \param IN option
 * \param IN width
 * \param IN valueP Current color ???
 * \param IN action Button callback procedure
 * \param IN data ???
 * \return bb handle for created button
 */

wButton_p wColorSelectButtonCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long 	option,
    wPos_t 	width,
    wDrawColor *valueP,
    wColorSelectButtonCallBack_p action,
    void 	* data)
{
    wButton_p b;
    colorData_t * cd;
    cd = malloc(sizeof(colorData_t));
    cd->valueP = valueP;
    cd->action = action;
    cd->data = data;
    cd->labelStr = labelStr;
    cd->color = (valueP?*valueP:0);

    b = wlibAlloc(parent, B_BUTTON, x, y, labelStr, sizeof *b, cd);
    b->option = option;
    wlibComputePos((wControl_p)b);

    b->widget = gtk_color_button_new();
    g_signal_connect(GTK_OBJECT(b->widget), "color-set",
                     G_CALLBACK(colorChange), cd);

    gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);

    if (option & BB_DEFAULT) {
        gtk_widget_set_can_default(b->widget, GTK_CAN_DEFAULT);
        gtk_widget_grab_default(b->widget);
        gtk_window_set_default(GTK_WINDOW(parent->gtkwin), b->widget);
    }

    wlibControlGetSize((wControl_p)b);

    gtk_widget_show(b->widget);
    wlibAddButton((wControl_p)b);
    wlibAddHelpString(b->widget, helpStr);
    wColorSelectButtonSetColor(b, (valueP?*valueP:0));
    
    if (labelStr) {
        ((wControl_p)b)->labelW = wlibAddLabel((wControl_p)b, labelStr);
    }
    return b;
}
