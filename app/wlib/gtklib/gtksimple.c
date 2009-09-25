/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtksimple.c,v 1.6 2009-09-25 05:38:15 dspagnol Exp $
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
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

static int windowxxx = 0;
/*
 *****************************************************************************
 *
 * Message Boxes
 *
 *****************************************************************************
 */

struct wMessage_t {
		WOBJ_COMMON
		GtkWidget * labelWidget;
		const char * message;
		wPos_t labelWidth;
		};

EXPORT void wMessageSetValue(
		wMessage_p b,
		const char * arg )
{
	if (b->widget == 0) abort();
	gtk_label_set( GTK_LABEL( b->labelWidget ), gtkConvertInput(arg) );
}


EXPORT void wMessageSetWidth(
		wMessage_p b,
		wPos_t width )
{
	b->labelWidth = width;
#ifndef GTK1
	gtk_widget_set_size_request( b->widget, width, -1 );
#else
	gtk_widget_set_usize( b->widget, width, -1 );
#endif
}


EXPORT wPos_t wMessageGetHeight(
		long flags )
{
	return 14;
}

/**
 * Create a window for a simple text. 
 *
 * \param IN parent Handle of parent window
 * \param IN x position in x direction
 * \param IN y position in y direction
 * \param IN labelStr ???
 * \param IN width horizontal size of window
 * \param IN message message to display ( null terminated )
 * \param IN flags display options
 * \return handle for created window
 */

EXPORT wMessage_p wMessageCreateEx(
		wWin_p	parent,
		wPos_t	x,
		wPos_t	y,
		const char 	* labelStr,
		wPos_t	width,
		const char	*message, 
		long flags )
{
	wMessage_p b;
	GtkRequisition requisition;
	PangoFontDescription *fontDesc;
	int fontSize;

	b = (wMessage_p)gtkAlloc( parent, B_MESSAGE, x, y, NULL, sizeof *b, NULL );
	gtkComputePos( (wControl_p)b );
	b->message = message;
	b->labelWidth = width;

	b->labelWidget = gtk_label_new( message?gtkConvertInput(message):"" );

	/* do we need to set a special font? */
	if( wMessageSetFont( flags ))	{
		/* get the current font descriptor */
		fontDesc = (b->labelWidget)->style->font_desc;
		
		/* get the current font size */
		fontSize = PANGO_PIXELS(pango_font_description_get_size( fontDesc ));
		
		/* calculate the new font size */
		if( flags & BM_LARGE ) {
			pango_font_description_set_size( fontDesc, fontSize * 1.4 * PANGO_SCALE );
		} else {
			pango_font_description_set_size( fontDesc, fontSize * 0.7 * PANGO_SCALE );
		}			
		
		/* set the new font size */
		gtk_widget_modify_font( (GtkWidget *)b->labelWidget, fontDesc );
	}

	b->widget = gtk_fixed_new();
	gtk_widget_size_request( GTK_WIDGET(b->labelWidget), &requisition );
	gtk_container_add( GTK_CONTAINER(b->widget), b->labelWidget );
	
	gtk_widget_set_size_request( b->widget, width?width:requisition.width, requisition.height );
	gtkControlGetSize( (wControl_p)b );
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );

	gtk_widget_show( b->widget );
	gtk_widget_show( b->labelWidget );
	gtkAddButton( (wControl_p)b );

	/* Reset font size to normal */
	if( wMessageSetFont( flags ))	{
		if( flags & BM_LARGE ) {
			pango_font_description_set_size(fontDesc, fontSize * PANGO_SCALE);
		} else {
			pango_font_description_set_size(fontDesc, fontSize * PANGO_SCALE);
		}
	}
	return b;
}

/*
 *****************************************************************************
 *
 * Lines
 *
 *****************************************************************************
 */

struct wLine_t {
		WOBJ_COMMON
		wBool_t visible;
		int count;
		wLines_t * lines;
		};

static void linesRepaint( wControl_p b )
{
	wLine_p bl = (wLine_p)(b);
	int i;
	wWin_p win = (wWin_p)(bl->parent);
	GdkDrawable * window;
	GdkColor *black;

	if (!bl->visible)
		return;
if (windowxxx)
	window = win->gtkwin->window;
else
	window = win->widget->window;

	/* get the GC attached to the panel in main() */
	black = gtkGetColor( wDrawColorBlack, TRUE );
	gdk_gc_set_foreground( win->gc, black );
	gdk_gc_set_function( win->gc, GDK_COPY );
	for (i=0; i<bl->count; i++) {
		if (win->gc_linewidth != bl->lines[i].width) {
			win->gc_linewidth = bl->lines[i].width;
			gdk_gc_set_line_attributes( win->gc, win->gc_linewidth, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER );
		}
		gdk_draw_line( window, win->gc,
				bl->lines[i].x0, bl->lines[i].y0,
				bl->lines[i].x1, bl->lines[i].y1 );
	}
}


void gtkLineShow(
		wLine_p bl,
		wBool_t visible )
{
	bl->visible = visible;
}


wLine_p wLineCreate(
		wWin_p	parent,
		const char	* labelStr,
		int	count,
		wLines_t * lines )
{
	wLine_p b;
	int i;
	b = (wLine_p)gtkAlloc( parent, B_LINES, 0, 0, labelStr, sizeof *b, NULL );
	if (parent->gc == NULL) {
		parent->gc = gdk_gc_new( parent->gtkwin->window );
		gdk_gc_copy( parent->gc, parent->gtkwin->style->base_gc[GTK_STATE_NORMAL] );
		parent->gc_linewidth = 0;
		gdk_gc_set_line_attributes( parent->gc, parent->gc_linewidth, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER );
	}
	b->visible = TRUE;
	b->count = count;
	b->lines = lines;

	b->w = b->h = 0;
	for ( i=0; i<count; i++ ) {
		if (lines[i].x0 > b->w)
			b->w = lines[i].x0;
		if (lines[i].y0 > b->h)
			b->h = lines[i].y0;
		if (lines[i].x1 > b->w)
			b->w = lines[i].x1;
		if (lines[i].y1 > b->h)
			b->h = lines[i].y1;
	}
	b->repaintProc = linesRepaint;
	gtkAddButton( (wControl_p)b );
	b->widget = 0;
	return b;
}


/*
 *****************************************************************************
 *
 * Boxes
 *
 *****************************************************************************
 */

struct wBox_t {
		WOBJ_COMMON
		wBoxType_e boxTyp;
		};

#define B (1)
#define W (2)
#define SETCOLOR( S, N ) \
		if ( lastColor != colors[style][S][N] ) { \
			lastColor = colors[style][S][N]; \
			gdk_gc_set_foreground( win->gc, (lastColor==B)?black:white ); \
		}

EXPORT void wBoxSetSize(
		wBox_p b,	/* */
		wPos_t w,	/* */
		wPos_t h )	/* */
{
	b->w = w;
	b->h = h;
}


EXPORT void gtkDrawBox(
		wWin_p win,
		wBoxType_e style,
		wPos_t x,
		wPos_t y,
		wPos_t w,
		wPos_t h )
{
	wPos_t x0, y0, x1, y1;
	char lastColor;
	GdkColor *white;
	GdkColor *black;
	GdkDrawable * window;
	static char colors[8][4][2] = {
		{ /* ThinB  */ {B,0}, {B,0}, {B,0}, {B,0} },
		{ /* ThinW  */ {W,0}, {W,0}, {W,0}, {W,0} },
		{ /* AboveW */ {W,0}, {W,0}, {B,0}, {B,0} },
		{ /* BelowW */ {B,0}, {B,0}, {W,0}, {W,0} },
		{ /* ThickB */ {B,B}, {B,B}, {B,B}, {B,B} },
		{ /* ThickW */ {W,W}, {W,W}, {W,W}, {W,W} },
		{ /* RidgeW */ {W,B}, {W,B}, {B,W}, {B,W} },
		{ /* TroughW*/ {B,W}, {B,W}, {W,B}, {W,B} } };

if (windowxxx)
	window = win->gtkwin->window;
else
	window = win->widget->window;
	white = gtkGetColor( wDrawColorWhite, TRUE );
	black = gtkGetColor( wDrawColorBlack, TRUE );
	win->gc_linewidth = 0;
	gdk_gc_set_line_attributes( win->gc, 0, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER );
	gdk_gc_set_function( win->gc, GDK_COPY );
	x0 = x;
	x1 = x+w;
	y0 = y;
	y1 = y+h;
	lastColor = colors[style][0][0];
	gdk_gc_set_foreground( win->gc, (lastColor==B)?black:white );
	gdk_draw_line( window, win->gc, x0, y0, x0, y1 );
	SETCOLOR( 1, 0 );
	gdk_draw_line( window, win->gc, x0+1, y0, x1, y0 );
	SETCOLOR( 2, 0 );
	gdk_draw_line( window, win->gc, x1, y1, x0+1, y1 );
	SETCOLOR( 3, 0 );
	gdk_draw_line( window, win->gc, x1, y1-1, x1, y0+1 );
	if (style < wBoxThickB)
		return;
	x0++; y0++; x1--; y1--;
	SETCOLOR( 0, 1 );
	gdk_draw_line( window, win->gc, x0, y0, x0, y1 );
	SETCOLOR( 1, 1 );
	gdk_draw_line( window, win->gc, x0+1, y0, x1, y0 );
	SETCOLOR( 2, 1 );
	gdk_draw_line( window, win->gc, x1, y1, x0+1, y1 );
	SETCOLOR( 3, 1 );
	gdk_draw_line( window, win->gc, x1, y1-1, x1, y0+1 );
	gdk_gc_set_foreground( win->gc, black );
}


static void boxRepaint( wControl_p b )
{
	wBox_p bb = (wBox_p)(b);
	wWin_p win = bb->parent;

	gtkDrawBox( win, bb->boxTyp, bb->realX, bb->realY, bb->w, bb->h );
}


wBox_p wBoxCreate(
		wWin_p	parent,
		wPos_t	bx,
		wPos_t	by,
		const char	* labelStr,
		wBoxType_e boxTyp,
		wPos_t	bw,
		wPos_t	bh )
{
	wBox_p b;
	b = (wBox_p)gtkAlloc( parent, B_BOX, bx, by, labelStr, sizeof *b, NULL );
	gtkComputePos( (wControl_p)b );
	b->boxTyp = boxTyp;
	b->w = bw;
	b->h = bh;
	if (parent->gc == NULL) {
		parent->gc = gdk_gc_new( parent->gtkwin->window );
		gdk_gc_copy( parent->gc, parent->gtkwin->style->base_gc[GTK_STATE_NORMAL] );
		parent->gc_linewidth = 0;
		gdk_gc_set_line_attributes( parent->gc, parent->gc_linewidth, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER );
	}
	b->repaintProc = boxRepaint;
	gtkAddButton( (wControl_p)b );
	return b;
}

