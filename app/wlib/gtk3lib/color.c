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
#define RGBA(RGB,A) ( ((long)((RGB)&0xFFFFFF)) | ((long)((A)&0xFF)<<24) )

//#define MAX_COLOR_DISTANCE (3)

//static GArray *colorMap_garray = NULL; // Change to use glib array

//static colorMap_t colorMap[] = {
//    { 255, 255, 255 },	/* White */
//    {   0,   0,   0 },	/* Black */
//    { 255,   0,   0 },	/* Red */
//    {   0, 255,   0 },	/* Green */
//    {   0,   0, 255 },	/* Blue */
//    { 255, 255,   0 },	/* Yellow */
//    { 255,   0, 255 },	/* Purple */
//    {   0, 255, 255 },	/* Aqua */
//    { 128,   0,   0 },	/* Dk. Red */
//    {   0, 128,   0 },	/* Dk. Green */
//    {   0,   0, 128 },	/* Dk. Blue */
//    { 128, 128,   0 },	/* Dk. Yellow */
//    { 128,   0, 128 },	/* Dk. Purple */
//    {  65, 105, 225 },      /* Royal Blue */
//    {   0, 191, 255 },	/* DeepSkyBlue */
//    { 125, 206, 250 },	/* LightSkyBlue */
//    {  70, 130, 180 },	/* Steel Blue */
//    { 176, 224, 230 },	/* Powder Blue */
//    { 127, 255, 212 },	/* Aquamarine */
//    {  46, 139,  87 },	/* SeaGreen */
//    { 152, 251, 152 },	/* PaleGreen */
//    { 124, 252,   0 },	/* LawnGreen */
//    {  50, 205,  50 },	/* LimeGreen */
//    {  34, 139,  34 },	/* ForestGreen */
//    { 255, 215,   0 },	/* Gold */
//    { 188, 143, 143 },	/* RosyBrown */
//    { 139, 69, 19 },	/* SaddleBrown */
//    { 245, 245, 220 },	/* Beige */
//    { 210, 180, 140 },	/* Tan */
//    { 210, 105, 30 },	/* Chocolate */
//    { 165, 42, 42 },	/* Brown */
//    { 255, 165, 0 },	/* Orange */
//    { 255, 127, 80 },	/* Coral */
//    { 255, 99, 71 },	/* Tomato */
//    { 255, 105, 180 },	/* HotPink */
//    { 255, 192, 203 },	/* Pink */
//    { 176, 48, 96 },	/* Maroon */
//    { 238, 130, 238 },	/* Violet */
//    { 160, 32, 240 },	/* Purple */
//    {  16,  16,  16 },	/* Gray */
//    {  32,  32,  32 },	/* Gray */
//    {  48,  48,  48 },	/* Gray */
//    {  64,  64,  64 },	/* Gray */
//    {  80,  80,  80 },	/* Gray */
//    {  96,  96,  96 },	/* Gray */
//    { 112, 112, 122 },	/* Gray */
//    { 128, 128, 128 },	/* Gray */
//    { 144, 144, 144 },	/* Gray */
//    { 160, 160, 160 },	/* Gray */
//    { 176, 176, 176 },	/* Gray */
//   { 192, 192, 192 },	/* Gray */
//    { 208, 208, 208 },	/* Gray */
//    { 224, 224, 224 },	/* Gray */
//    { 240, 240, 240 },	/* Gray */
//    {   0,   0,   0 }	/* BlackPixel */
//};

//#define NUM_GRAYS (16)

//static GdkVisual * gdkVisual;

//static char lastColorChar = '!';

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


    if (percent <= 0) {
        return wDrawColorBlack;
    } else if (percent > 100) {
        return wDrawColorWhite;
    }

    return RGB((percent*256/100), percent*256/100, percent*256/100);

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
	return rgb0;
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
    return color;
}

/**
 * Get the color definition from the palette index
 *
 * \param color IN index into palette
 * \param normal IN normal or inverted color
 * \return  the selected color definition
 */

wDrawColor wlibGetColor(
    wDrawColor color,
    wBool_t normal)
{

    if (normal) {
        return RGBA(color,255);
    } else {
        return RGBA(0xFFFFFF-color,255);
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
    long rgb;

    GdkRGBA rgba;

    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &rgba);
    //gtk_color_button_get_color(widget, &newcolor);

    rgb = RGB((int)(rgba.red/256), (int)(rgba.green/256),
              (int)(rgba.blue/256));

    if (cd->valueP) {
        *(cd->valueP) = rgb;
    }

    if (cd->action) {
        cd->action(cd->data, rgb);
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

	GdkRGBA rgba;

	rgba.red = (color&0x00FF0000)>>16;
	rgba.green = (color&0x0000FF00)>>8;
	rgba.blue = (color&0x000000FF);
	rgba.alpha = 1.0;

    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(bb->widget),
                               &rgba);
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

    if (option&BO_USETEMPLATE) {
    	char name[256];
    	sprintf(name,"%s",helpStr);
    	b->widget = wlibWidgetFromId( parent, name );
    	if (b->widget) 
            b->fromTemplate = TRUE;
    } else {
    	b->widget = gtk_color_button_new();
        gtk_widget_set_size_request(GTK_WIDGET(b->widget), 22, 22);
        gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
        if (option & BB_DEFAULT) {
            gtk_widget_set_can_default(b->widget, TRUE);
            gtk_widget_grab_default(b->widget);
            gtk_window_set_default(GTK_WINDOW(parent->gtkwin), b->widget);
        }
    }
    //GtkStyleContext *stylecontext;
   // stylecontext = gtk_widget_get_style_context(b->widget);
   // stylecontext->xthickness = 1;
   // stylecontext->ythickness = 1;
   // gtk_widget_set_style_context(b->widget, stylecontext);

    g_signal_connect(b->widget, "color-set",
                     G_CALLBACK(colorChange), cd);

    if (option&BO_CONTROLGRID) {
    	g_object_ref(b->widget);
    	b->useGrid = TRUE;
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
