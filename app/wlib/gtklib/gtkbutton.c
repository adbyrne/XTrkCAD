/** \file gtkbutton.c
 * Toolbar button creation and handling
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkbutton.c,v 1.4 2007-09-28 11:17:34 m_fischer Exp $
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

#define MIN_BUTTON_WIDTH (80)

/*
 *****************************************************************************
 *
 * Simple Buttons
 *
 *****************************************************************************
 */

struct wButton_t {
		WOBJ_COMMON
		GtkLabel * labelG;
		GtkPixmap * pixmapG;
		wButtonCallBack_p action;
		int busy;
		int recursion;
		};


void wButtonSetBusy( wButton_p bb, int value ) {
	bb->recursion++;
	gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(bb->widget), value );
	bb->recursion--;
	bb->busy = value;
}


void gtkSetLabel(
		GtkWidget *widget,
		long option,
		const char * labelStr,
		GtkLabel * * labelG,
		GtkPixmap * * pixmapG )
{

	wIcon_p bm;
	GdkPixmap * pixmap;
	GdkBitmap * mask;
	GtkWidget * hbox;
	if (widget == 0) abort();
	if (labelStr){
		 if (option&BO_ICON) {
			 bm = (wIcon_p)labelStr;
			 pixmap = gtkMakeIcon( widget, bm, &mask );
			 if (*pixmapG==NULL) {
				*pixmapG = (GtkPixmap*)gtk_pixmap_new( pixmap, mask );
				hbox = gtk_hbox_new( FALSE, 0 );
				gtk_container_add( GTK_CONTAINER(hbox), (GtkWidget*)*pixmapG );
				gtk_container_add( GTK_CONTAINER(widget), hbox );
				/*gtk_container_add( GTK_CONTAINER(widget), (GtkWidget*)*pixmapG );*/
				gtk_widget_show( hbox );
				gtk_widget_show( (GtkWidget*)*pixmapG );
			 } else {
				 gtk_pixmap_set( *pixmapG, pixmap, mask );
			 }
			 gdk_pixmap_unref( pixmap );
			 gdk_bitmap_unref( mask );
		 } else {
			if (*labelG==NULL) {
				*labelG = (GtkLabel*)gtk_label_new( gtkConvertInput(labelStr) );
				gtk_container_add( GTK_CONTAINER(widget), (GtkWidget*)*labelG );
				gtk_widget_show( (GtkWidget*)*labelG );
			} else {
				 gtk_label_set( *labelG, gtkConvertInput(labelStr) );
			}
		 }
	}
}

void wButtonSetLabel( wButton_p bb, const char * labelStr) {
	gtkSetLabel( bb->widget, bb->option, labelStr, &bb->labelG, &bb->pixmapG );
}



void gtkButtonDoAction( 
		wButton_p bb )
{
	if (bb->action)
		bb->action( bb->data );
}


static void pushButt(
		GtkWidget *widget,
		gpointer value )
{
	wButton_p b = (wButton_p)value;
	if (debugWindow >= 2) printf("%s button pushed\n", b->labelStr?b->labelStr:"No label" );
	if (b->recursion)
		return;
	if (b->action)
		b->action(b->data);
	if (!b->busy) {
		b->recursion++;
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(b->widget), FALSE );
		b->recursion--;
	}
}

wButton_p wButtonCreate(
		wWin_p	parent,
		wPos_t	x,
		wPos_t	y,
		const char 	* helpStr,
		const char	* labelStr,
		long 	option,
		wPos_t 	width,
		wButtonCallBack_p action,
		void 	* data )
{
	wButton_p b;
	b = gtkAlloc( parent, B_BUTTON, x, y, labelStr, sizeof *b, data );
	b->option = option;
	b->action = action;
	gtkComputePos( (wControl_p)b );

	b->widget = gtk_toggle_button_new();
				
	gtk_signal_connect (GTK_OBJECT(b->widget), "clicked",
						GTK_SIGNAL_FUNC(pushButt), b );
	if (width > 0)
#ifndef GTK1
		gtk_widget_set_size_request( b->widget, width, -1 );
#else
		gtk_widget_set_usize( b->widget, width, -1 );
#endif
	wButtonSetLabel( b, labelStr );

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	if (option & BB_DEFAULT) {
		GTK_WIDGET_SET_FLAGS( b->widget, GTK_CAN_DEFAULT );
		gtk_widget_grab_default( b->widget );
		gtk_window_set_default( GTK_WINDOW(parent->gtkwin), b->widget );
	}
	gtkControlGetSize( (wControl_p)b );
	if (width == 0 && b->w < MIN_BUTTON_WIDTH && (b->option&BO_ICON)==0) {
		b->w = MIN_BUTTON_WIDTH;
#ifndef GTK1
		gtk_widget_set_size_request( b->widget, b->w, b->h );
#else
		gtk_widget_set_usize( b->widget, b->w, b->h );
#endif
	}
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	gtkAddHelpString( b->widget, helpStr );
	return b;
}


/*
 *****************************************************************************
 *
 * Choice Boxes
 *
 *****************************************************************************
 */

struct wChoice_t {
		WOBJ_COMMON
		long *valueP;
		wChoiceCallBack_p action;
		int recursion;
		};


static long choiceGetValue(
		wChoice_p bc )
{
	GList * child, * children;
	long value, inx;
	if (bc->type == B_TOGGLE)
		value = 0;
	else
		value = -1;
	for ( children=child=gtk_container_children(GTK_CONTAINER(bc->widget)),inx=0; child; child=child->next,inx++ ) {
		if ( GTK_TOGGLE_BUTTON(child->data)->active ) {
			if (bc->type == B_TOGGLE) {
				value |= (1<<inx);
			} else {
				value = inx;
			}
		}
	}
	if ( children )
		g_list_free( children );
	return value;
}

EXPORT void wRadioSetValue(
		wChoice_p bc,		/* Radio box */
		long value )		/* Value */
/*
*/
{
	GList * child, * children;
	long inx;
	for ( children=child=gtk_container_children(GTK_CONTAINER(bc->widget)),inx=0; child; child=child->next,inx++ ) {
		if (inx == value) {
			bc->recursion++;
			gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(child->data), TRUE );
			bc->recursion--;
		}
	}
	if ( children )
		g_list_free( children );
}


EXPORT long wRadioGetValue(
		wChoice_p bc )		/* Radio box */
/*
*/
{
	return choiceGetValue(bc);
}


EXPORT void wToggleSetValue(
		wChoice_p bc,		/* Toggle box */
		long value )		/* Values */
/*
*/
{
	GList * child, * children;
	long inx;
	bc->recursion++;
	for ( children=child=gtk_container_children(GTK_CONTAINER(bc->widget)),inx=0; child; child=child->next,inx++ ) {
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(child->data), (value&(1<<inx))!=0 ); 
	}
	if ( children )
		g_list_free( children );
	bc->recursion--;
}


EXPORT long wToggleGetValue(
		wChoice_p b )		/* Toggle box */
/*
*/
{
	return choiceGetValue(b);
}


static int pushChoice(
		GtkWidget *widget,
		gpointer b )
{
	wChoice_p bc = (wChoice_p)b;
	long value = choiceGetValue( bc );
	if (debugWindow >= 2) printf("%s choice pushed = %ld\n", bc->labelStr?bc->labelStr:"No label", value );
	if ( bc->type == B_RADIO && !(GTK_TOGGLE_BUTTON(widget))->active )
		return 1;
	if (bc->recursion)
		return 1;
	if (bc->valueP)
		*bc->valueP = value;
	if (bc->action)
		bc->action( value, bc->data);
	return 1;
}


static void choiceRepaint(
		wControl_p b )
{
	wChoice_p bc = (wChoice_p)b;
	if ( GTK_WIDGET_VISIBLE( b->widget ) )
		gtkDrawBox( bc->parent, wBoxBelow, bc->realX-1, bc->realY-1, bc->w+1, bc->h+1 );
}


EXPORT wChoice_p wRadioCreate(
		wWin_p	parent,		/* Parent window */
		wPos_t	x,		/* X-position */
		wPos_t	y,		/* Y-position */
		const char 	* helpStr,	/* Help string */
		const char	* labelStr,	/* Label */
		long	option,		/* Options */
		const char	**labels,	/* Labels */
		long	*valueP,	/* Selected value */
		wChoiceCallBack_p action,	/* Callback */
		void 	*data )		/* Context */
/*
*/
{
	wChoice_p b;
	const char ** label;
	GtkWidget *butt0=NULL, *butt;

	if ((option & BC_NOBORDER)==0) {
		if (x>=0)
			x++;
		else
			x--;
		if (y>=0)
			y++;
		else
			y--;
	}
	b = gtkAlloc( parent, B_RADIO, x, y, labelStr, sizeof *b, data );
	b->option = option;
	b->action = action;
	b->valueP = valueP;
	gtkComputePos( (wControl_p)b );

	if (option&BC_HORZ)
		b->widget = gtk_hbox_new( FALSE, 10 );
	else
		b->widget = gtk_vbox_new( FALSE, 10 );
	if (b->widget == 0) abort();
	for ( label=labels; *label; label++ ) {
		butt = gtk_radio_button_new_with_label( 
				butt0?gtk_radio_button_group(GTK_RADIO_BUTTON(butt0)):NULL, *label );
		if (butt0==NULL)
			butt0 = butt;
		gtk_box_pack_start( GTK_BOX(b->widget), butt, TRUE, TRUE, 0 );
		gtk_widget_show( butt );
		gtk_signal_connect (GTK_OBJECT(butt), "toggled",
						GTK_SIGNAL_FUNC( pushChoice ), b );
		gtkAddHelpString( butt, helpStr );
	}
	if (option & BB_DEFAULT) {
		GTK_WIDGET_SET_FLAGS( b->widget, GTK_CAN_DEFAULT );
		gtk_widget_grab_default( b->widget );
		/*gtk_window_set_default( GTK_WINDOW(parent->gtkwin), b->widget );*/
	}
	if (valueP)
		wRadioSetValue( b, *valueP );

	if ((option & BC_NOBORDER)==0) {
		if (parent->gc == NULL) {
			parent->gc = gdk_gc_new( parent->gtkwin->window );
			gdk_gc_copy( parent->gc, parent->gtkwin->style->base_gc[GTK_STATE_NORMAL] );
			parent->gc_linewidth = 0;
			gdk_gc_set_line_attributes( parent->gc, parent->gc_linewidth, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER );
		}
		b->repaintProc = choiceRepaint;
		b->w += 2;
		b->h += 2;
	}

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	gtkControlGetSize( (wControl_p)b );
	if (labelStr)
		b->labelW = gtkAddLabel( (wControl_p)b, labelStr );
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	return b;
}

wChoice_p wToggleCreate(
		wWin_p	parent,		/* Parent window */
		wPos_t	x,		/* X-position */
		wPos_t	y,		/* Y-position */
		const char 	* helpStr,	/* Help string */
		const char	* labelStr,	/* Label */
		long	option,		/* Options */
		const char	**labels,	/* Labels */
		long	*valueP,	/* Selected value */
		wChoiceCallBack_p action,	/* Callback */
		void 	*data )		/* Context */
/*
*/
{
	wChoice_p b;
	const char ** label;
	GtkWidget *butt;

	if ((option & BC_NOBORDER)==0) {
		if (x>=0)
			x++;
		else
			x--;
		if (y>=0)
			y++;
		else
			y--;
	}
	b = gtkAlloc( parent, B_TOGGLE, x, y, labelStr, sizeof *b, data );
	b->option = option;
	b->action = action;
	gtkComputePos( (wControl_p)b );

	if (option&BC_HORZ)
		b->widget = gtk_hbox_new( FALSE, 10 );
	else
		b->widget = gtk_vbox_new( FALSE, 10 );
	if (b->widget == 0) abort();
	for ( label=labels; *label; label++ ) {
		butt = gtk_check_button_new_with_label( *label );
		gtk_box_pack_start( GTK_BOX(b->widget), butt, TRUE, TRUE, 0 );
		gtk_widget_show( butt );
		gtk_signal_connect (GTK_OBJECT(butt), "toggled",
						GTK_SIGNAL_FUNC( pushChoice ), b );
		gtkAddHelpString( butt, helpStr );
	}
	if (valueP)
		wToggleSetValue( b, *valueP );

	if ((option & BC_NOBORDER)==0) {
		if (parent->gc == NULL) {
			parent->gc = gdk_gc_new( parent->gtkwin->window );
			gdk_gc_copy( parent->gc, parent->gtkwin->style->base_gc[GTK_STATE_NORMAL] );
			parent->gc_linewidth = 0;
			gdk_gc_set_line_attributes( parent->gc, parent->gc_linewidth, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER );
		}
		b->repaintProc = choiceRepaint;
		b->w += 2;
		b->h += 2;
	}

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	gtkControlGetSize( (wControl_p)b );
	if (labelStr)
		b->labelW = gtkAddLabel( (wControl_p)b, labelStr );
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	return b;
}
