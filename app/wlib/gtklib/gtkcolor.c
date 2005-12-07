/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkcolor.c,v 1.1 2005-12-07 15:48:44 rc-flyer Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>

#include "gtkint.h"

#include "square10.bmp"

EXPORT wDrawColor wDrawColorWhite;
EXPORT wDrawColor wDrawColorBlack;

#define RGB(R,G,B) ( (((long)((R)&0xFF))<<16) | (((long)((G)&0xFF))<<8) | ((long)((B)&0xFF)) )

#define MAX_COLOR_DISTANCE (3)

typedef struct {
		unsigned char red;
		unsigned char green;
		unsigned char blue;
		GdkColor normalColor;
		GdkColor invertColor;
		long rgb;
		int colorChar;
		} colorMap_t;
static dynArr_t colorMap_da;
#define colorMap(N) DYNARR_N( colorMap_t, colorMap_da, N )

#ifdef LATER
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
#endif

#define NUM_GRAYS (16)

static GdkColormap * gtkColorMap;

static char lastColorChar = '!';

/*****************************************************************************
 *
 * 
 *
 */


EXPORT wDrawColor wDrawColorGray(
		int percent )
{
	int n;
	long rgb;
	n = (percent * (NUM_GRAYS+1)) / 100;
	if ( n <= 0 )
		return wDrawColorBlack;
	else if ( n > NUM_GRAYS )
		return wDrawColorWhite;
	else {
		n = (n*256)/NUM_GRAYS;
		rgb = RGB( n, n, n );
		return wDrawFindColor( rgb );
	}
}


void gtkGetColorMap( void )
{
	if (gtkColorMap)
		return;
	gtkColorMap = gtk_widget_get_colormap( gtkMainW->widget );
	return;
}


EXPORT wDrawColor wDrawFindColor(
		long rgb0 )
{
	wDrawColor cc;
	int r0, g0, b0;
	int d0, d1;
	long rgb1;
	colorMap_t * cm_p;

	gtkGetColorMap();

	cc = wDrawColorBlack;
	r0 = (int)(rgb0>>16)&0xFF;
	g0 = (int)(rgb0>>8)&0xFF;
	b0 = (int)(rgb0)&0xFF;
	d0 = 256*3;

	for (cm_p=&colorMap(0); cm_p<&colorMap(colorMap_da.cnt); cm_p++ ) {
		rgb1 = cm_p->rgb;
		d1 = abs(r0-cm_p->red) + abs(g0-cm_p->green) + abs(b0-cm_p->blue);
		if (d1 == 0)
			return cm_p-&colorMap(0);
		if (d1 < d0) {
			d0 = d1;
			cc = cm_p-&colorMap(0);
		}
	}
	if (d0 <= MAX_COLOR_DISTANCE) {
		return cc;
	}
	DYNARR_APPEND( colorMap_t, colorMap_da, 10 );
	cm_p = &colorMap(colorMap_da.cnt-1);
	cm_p->red = r0;
	cm_p->green = g0;
	cm_p->blue = b0;
	cm_p->rgb = RGB( cm_p->red, cm_p->green, cm_p->blue );
	cm_p->normalColor.red = cm_p->red*65535/255;
	cm_p->normalColor.green = cm_p->green*65535/255;
	cm_p->normalColor.blue = cm_p->blue*65535/255;
	gdk_color_alloc( gtkColorMap, &cm_p->normalColor );
	cm_p->invertColor = cm_p->normalColor;
	cm_p->invertColor.pixel ^= colorMap(wDrawColorWhite).normalColor.pixel;
	cm_p->colorChar = lastColorChar++;
	if (lastColorChar >= 0x7F)
		lastColorChar = '!'+1;
	else if (lastColorChar == '"')
		lastColorChar++;
	return colorMap_da.cnt-1;
}



EXPORT long wDrawGetRGB(
		wDrawColor color )
{
	gtkGetColorMap();

	if (color < 0 || color >= colorMap_da.cnt)
		abort();
	return colorMap(color).rgb;
}


EXPORT GdkColor* gtkGetColor(
		wDrawColor color,
		wBool_t normal )
{
	gtkGetColorMap();

	if (color < 0 || color >= colorMap_da.cnt)
		abort();
	if ( normal )
		return &colorMap(color).normalColor;
	else
		return &colorMap(color).invertColor;
}


EXPORT int gtkGetColorChar(
		wDrawColor color )
{
	/*gtkGetColorMap();*/

	if (color < 0 || color >= colorMap_da.cnt)
		abort();
	return colorMap(color).colorChar;
}


EXPORT int gtkMapPixel( 
		long pixel )
{
	colorMap_t * cm_p;
	
	for (cm_p=&colorMap(0); cm_p<&colorMap(colorMap_da.cnt); cm_p++ ) {
		if ( cm_p->normalColor.pixel == pixel ) {
			return cm_p->colorChar;
		}
	}
	return colorMap(wDrawColorBlack).colorChar;
}


EXPORT void gtkPrintColorMap(
		FILE * f,
		int w,
		int h )
{
	wDrawColor color;
	int cnt = colorMap_da.cnt;
	if (cnt > (0x7E - ' ') )
		cnt = 0x7E - ' ';
	fprintf( f, "\"%d %d %d 1\"\n", w, h, cnt );
	fprintf( f, "/* colors */\n" );
	for ( color = 0; color < colorMap_da.cnt; color++ ) {
		fprintf( f, "\"%c c #%6.6x\",\n", colorMap(color).colorChar, (int)colorMap(color).rgb );
	}
}

/*
 *****************************************************************************
 *
 * Color Selection Dialog
 *
 *****************************************************************************
 */


static int colorSelectValid;
static int colorSelectOk(
		GtkWidget * widget,
		GtkWidget * * window )
{
	gtkDoModal( NULL, FALSE );
	gtk_widget_hide( GTK_WIDGET(*window) );
	colorSelectValid = TRUE;
	return FALSE;
}


static int colorSelectCancel(
		GtkWidget * widget,
		GtkWidget * * window )
{
	gtkDoModal( NULL, FALSE );
	gtk_widget_hide( GTK_WIDGET(*window) );
	colorSelectValid = FALSE;
	if (widget == *window)
		/* Called by destroy event, window is gone */
		*window = NULL;
	return FALSE;
}


EXPORT wBool_t wColorSelect(
		const char * title,
		wDrawColor * color )
{
	static GtkWidget * colorSelectD = NULL;
	long rgb;
	gdouble colors[3];

	if (colorSelectD == NULL) {
		colorSelectD = gtk_color_selection_dialog_new( title );
		gtk_signal_connect( GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(colorSelectD)->ok_button), "clicked", (GtkSignalFunc)colorSelectOk, (gpointer)&colorSelectD );
		gtk_signal_connect( GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(colorSelectD)->cancel_button), "clicked", (GtkSignalFunc)colorSelectCancel, (gpointer)&colorSelectD );
		gtk_signal_connect( GTK_OBJECT(colorSelectD), "destroy", (GtkSignalFunc)colorSelectCancel, (gpointer)&colorSelectD );
	} else {
		gtk_window_set_title( GTK_WINDOW(colorSelectD), title );
	}
	colors[0] = colorMap(*color).red/255.0;
	colors[1] = colorMap(*color).green/255.0;
	colors[2] = colorMap(*color).blue/255.0;
	gtk_color_selection_set_color( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorSelectD)->colorsel), colors );
	gtk_widget_show( colorSelectD );
	gtkDoModal( NULL, TRUE );
	if (colorSelectValid) {
		gtk_color_selection_get_color( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorSelectD)->colorsel), colors );
		rgb = RGB( (int)(colors[0]*255), (int)(colors[1]*255), (int)(colors[2]*255) );
		*color = wDrawFindColor( rgb );
		return TRUE;
	}
	return FALSE;
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
		wColorSelectButtonCallBack_p action;
		const char * labelStr;
		void * data;
		wDrawColor color;
		wButton_p button;
		} colorData_t;

static void doColorButton(
		void * data )
{
	colorData_t * cd = (colorData_t*)data;
	wDrawColor newColor;

	newColor = cd->color;
	if (wColorSelect( cd->labelStr, &newColor )) {
		cd->color = newColor;
		wColorSelectButtonSetColor( cd->button, newColor );
		if (cd->valueP)
			*cd->valueP = newColor;
		if (cd->action)
			cd->action( cd->data, newColor );
	}
}


void wColorSelectButtonSetColor(
		wButton_p bb,
		wDrawColor color )
{
	wIcon_p bm;
	bm = wIconCreateBitMap( square10_width, square10_height, square10_bits, color );
	wButtonSetLabel( bb, (const char*)bm );
	((colorData_t*)((wControl_p)bb)->data)->color = color;
}


wDrawColor wColorSelectButtonGetColor(
		wButton_p bb )
{
	return ((colorData_t*)((wControl_p)bb)->data)->color;
}


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
		void 	* data )
{
	wButton_p bb;
	wIcon_p bm;
	colorData_t * cd;
	bm = wIconCreateBitMap( square10_width, square10_height, square10_bits, (valueP?*valueP:0) );
	cd = malloc( sizeof *cd );
	cd->valueP = valueP;
	cd->action = action;
	cd->data = data;
	cd->labelStr = labelStr;
	cd->color = (valueP?*valueP:0);
	bb = wButtonCreate( parent, x, y, helpStr, (const char*)bm, option|BO_ICON, width, doColorButton, cd );
	cd->button = bb;
	if (labelStr)
		((wControl_p)bb)->labelW = gtkAddLabel( (wControl_p)bb, labelStr );
	return bb;
}
