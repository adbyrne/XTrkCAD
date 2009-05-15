/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtksingle.c,v 1.2 2009-05-15 18:54:20 m_fischer Exp $
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
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>

#include "gtkint.h"

/*
 *****************************************************************************
 *
 * Text Boxes
 *
 *****************************************************************************
 */

struct wString_t {
		WOBJ_COMMON
		char * valueP;
		wIndex_t valueL;
		wStringCallBack_p action;
		wBool_t busy;
		};

void wStringSetValue(
		wString_p b,
		const char * arg )
{
	wBool_t busy;
	if (b->widget == 0) abort();
	busy = b->busy;
		b->busy = TRUE;
		gtk_entry_set_text( GTK_ENTRY(b->widget), arg );
		b->busy = busy;
}


void wStringSetWidth(
		wString_p b,
		wPos_t w )
{
#ifndef GTK1
	gtk_widget_set_size_request( b->widget, w, -1 );
#else
	gtk_widget_set_usize( b->widget, w, -1 );
#endif
	b->w = w;
}


const char * wStringGetValue(
		wString_p b )
{
	if (b->widget == 0) abort();
	return gtk_entry_get_text( GTK_ENTRY(b->widget) );
}


static void triggerString(
		wControl_p b )
{
	wString_p bs = (wString_p)b;
	const char * s;

	if (b == 0)
		return;
	if (bs->widget == 0) abort();
	s = gtk_entry_get_text( GTK_ENTRY(bs->widget) );
	if (debugWindow >= 2) printf("%s text = %s\n", bs->labelStr?bs->labelStr:"No label", s );
	if (s == NULL) 
		return;
	if (debugWindow >= 2) printf("triggerString( %s )\n", s );
	if (bs->action) {
		bs->busy = TRUE;
		bs->action( s, bs->data );
		bs->busy = FALSE;
	}
	gtkSetTrigger( NULL, NULL );
	return;
}


static void stringActivated(
		GtkEntry * widget,
		wString_p b )
{
	const char * s;
	if (b == 0)
		return;
	s = wStringGetValue(b);

	if (debugWindow >= 2) printf("%s text = %s\n", b->labelStr?b->labelStr:"No label", s );
	if (b->valueP)
		strcpy( b->valueP, s );
	if (b->action) {
		b->busy = TRUE;
		b->action( s, b->data );
		b->busy = FALSE;
	}
}

static void stringChanged(
		GtkEntry * widget,
		wString_p b )
{
	const char *new_value;
	if (b == 0)
		return;
	if (b->busy)
		return;
	new_value = wStringGetValue(b);
	if (b->valueP != NULL)
		strcpy( b->valueP, new_value );
	if (b->action)
		gtkSetTrigger( (wControl_p)b, triggerString );
	return;
}

wString_p wStringCreate(
		wWin_p	parent,
		wPos_t	x,
		wPos_t	y,
		const char 	* helpStr,
		const char	* labelStr,
		long	option,
		wPos_t	width,
		char	*valueP,
		wIndex_t valueL,
		wStringCallBack_p action,
		void 	*data )
{
	wString_p b;

	b = (wString_p)gtkAlloc( parent, B_TEXT, x, y, labelStr, sizeof *b, data );
	b->valueP = valueP;
	b->action = action;
	b->option = option;
	b->valueL = valueL;
	gtkComputePos( (wControl_p)b );

	if (valueL) {
		b->widget = (GtkWidget*)gtk_entry_new_with_max_length( valueL );
	} else {
		b->widget = (GtkWidget*)gtk_entry_new();
	}
	if (b->widget == 0) abort();

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	if ( width )
#ifndef GTK1
		gtk_widget_set_size_request( b->widget, width, -1 );
#else
		gtk_widget_set_usize( b->widget, width, -1 );
#endif
	gtkControlGetSize( (wControl_p)b );
	if (labelStr)
		b->labelW = gtkAddLabel( (wControl_p)b, labelStr );
	/*b->w += 4;*/
	/*b->h += 4;*/
	if (b->valueP)
		wStringSetValue( b, b->valueP );
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	gtkAddHelpString( b->widget, helpStr );
	gtk_signal_connect( GTK_OBJECT(b->widget), "changed", GTK_SIGNAL_FUNC(stringChanged), b );
	gtk_signal_connect( GTK_OBJECT(b->widget), "activate", GTK_SIGNAL_FUNC(stringActivated), b );
	if (option & BO_READONLY)
		gtk_entry_set_editable( GTK_ENTRY(b->widget), FALSE );
	return b;
}

/*
 *****************************************************************************
 *
 * Floating Point Value Boxes
 *
 *****************************************************************************
 */


struct wFloat_t {
		WOBJ_COMMON
		double low, high;
		double oldValue;
		double * valueP;
		wFloatCallBack_p action;
		wBool_t busy;
		};


void wFloatSetValue(
		wFloat_p b,
		double arg )
{
	char message[80];
	if (b->widget == 0) abort();
	sprintf( message, "%0.3f", arg );
	if (!b->busy) {
		b->busy = TRUE;
		gtk_entry_set_text( GTK_ENTRY(b->widget), message );
		b->busy = FALSE;
	}
	if (b->valueP)
		*b->valueP = arg;
}


double wFloatGetValue(
		wFloat_p b )
{
	double ret;
	const char * cp;
	if (b->widget == 0) abort();
	cp = gtk_entry_get_text( GTK_ENTRY(b->widget) );
	ret = atof( cp );
	return ret;
}


static void triggerFloat(
		wControl_p b )
{
	wFloat_p bf = (wFloat_p)b;
	const char * s;
	char * cp;
	double v;

	if (b == 0)
		return;
	if (bf->widget == 0) abort();
	s = gtk_entry_get_text( GTK_ENTRY(bf->widget) );
	if (debugWindow >= 2) printf("%s text = %s\n", bf->labelStr?bf->labelStr:"No label", s );
	if (s == NULL) 
		return;
	v = strtod( s, &cp );
	if (*cp!=0 || v < bf->low || v > bf->high)
		return;
	/*if (bf->oldValue == v)
		return;*/
	if (debugWindow >= 2) printf("triggerFloat( %0.3f )\n", v );
	bf->oldValue = v;
	if (bf->valueP)
		*bf->valueP = v;
	if (bf->action) {
		bf->busy = TRUE;
		bf->action( v, bf->data );
		bf->busy = FALSE;
	}
	gtkSetTrigger( NULL, NULL );
	return;
}


static void floatActivated(
		GtkEntry *widget,
		wFloat_p b )
{
	const char * s;
	char * cp;
	double v;
	char val_s[80];

	if (b == 0)
		return;
	if (b->widget == 0) abort();
	s = gtk_entry_get_text( GTK_ENTRY(b->widget) );
	if (debugWindow >= 2) printf("%s text = %s\n", b->labelStr?b->labelStr:"No label", s );
	if (s != NULL) {
		v = strtod( s, &cp );
		if (*cp != '\n' && *cp != '\0') {
			wNoticeEx( NT_ERROR, "The value you have entered is not a valid number\nPlease try again", "Ok", NULL );
		} else if (v < b->low || v > b->high) {
			sprintf( val_s, "Please enter a value between %0.3f and %0.3f", b->low, b->high );
			wNoticeEx( NT_ERROR, val_s, "Ok", NULL );
		} else {
			if (debugWindow >= 2) printf("floatActivated( %0.3f )\n", v );
			b->oldValue = v;
			if (b->valueP)
				*b->valueP = v;
			if (b->action) {
				gtkSetTrigger( NULL, NULL );
				b->busy = TRUE;
				b->action( v, b->data );
				b->busy = FALSE;
			}
			return;
		}
		sprintf( val_s, "%0.3f", b->oldValue);
		b->busy = TRUE;
		gtk_entry_set_text( GTK_ENTRY(b->widget), val_s );
		b->busy = FALSE;
	}
	return;
}

static void floatChanged(
		GtkEntry *widget,
		wFloat_p b )
{
	const char * s;
	char * cp;
	double v;

	if (b == 0)
		return;
	if (b->widget == 0) abort();
	if (b->busy)
		return;
	s = gtk_entry_get_text( GTK_ENTRY(b->widget) );
	if (s == NULL)
		return;
	if (debugWindow >= 2) printf("%s text = %s\n", b->labelStr?b->labelStr:"No label", s );
	if ( s[0] == '\0' ||
		 strcmp( s, "-" ) == 0 ||
		 strcmp( s, "." ) == 0 ) {
		v = 0;
	} else {
		v = strtod( s, &cp );
		if (*cp != '\0'
#ifdef LATER
				 || v < b->low || v > b->high
#endif
						) {
			wBeep();
			wFloatSetValue( b, b->oldValue );
			return;
		}
	}
	b->oldValue = v;
	if (b->valueP != NULL) {
		*b->valueP = v;
	}
	if (b->action)
		gtkSetTrigger( (wControl_p)b, triggerFloat );
	return;
}

wFloat_p wFloatCreate(
		wWin_p	parent,
		wPos_t	x,
		wPos_t	y,
		const char 	* helpStr,
		const char	* labelStr,
		long	option,
		wPos_t	width,
		double	low,
		double	high,
		double	*valueP,
		wFloatCallBack_p action,
		void 	*data )
{
	wFloat_p b;

	b = (wFloat_p)gtkAlloc( parent, B_TEXT, x, y, labelStr, sizeof *b, data );
	b->valueP =  valueP;
	b->action = action;
	b->option = option;
	b->low = low;
	b->high = high;
	gtkComputePos( (wControl_p)b );

	b->widget = (GtkWidget*)gtk_entry_new_with_max_length( 20 );
	if (b->widget == 0) abort();

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	if ( width )
#ifndef GTK1
		gtk_widget_set_size_request( b->widget, width, -1 );
#else
		gtk_widget_set_usize( b->widget, width, -1 );
#endif
	gtkControlGetSize( (wControl_p)b );
	if (labelStr)
		b->labelW = gtkAddLabel( (wControl_p)b, labelStr );
	/*b->w += 4;*/
	/*b->h += 4;*/
	if (b->valueP)
		wFloatSetValue( b, *b->valueP );
	else
		wFloatSetValue( b, b->low>0?b->low:0.0 );
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	gtkAddHelpString( b->widget, helpStr );
	gtk_signal_connect( GTK_OBJECT(b->widget), "changed", GTK_SIGNAL_FUNC(floatChanged), b );
	gtk_signal_connect( GTK_OBJECT(b->widget), "activate", GTK_SIGNAL_FUNC(floatActivated), b );
	if (option & BO_READONLY)
		gtk_entry_set_editable( GTK_ENTRY(b->widget), FALSE );
	return b;
}

/*
 *****************************************************************************
 *
 * Integer Value Boxes
 *
 *****************************************************************************
 */


struct wInteger_t {
		WOBJ_COMMON
		long low, high;
		long oldValue;
		long * valueP;
		wIntegerCallBack_p action;
		wBool_t busy;
		};


void wIntegerSetValue(
		wInteger_p b,
		long arg )
{
	char message[80];
	if (b->widget == 0) abort();
	sprintf( message, "%ld", arg );
	if (!b->busy) {
		b->busy = TRUE;
		gtk_entry_set_text( GTK_ENTRY(b->widget), message );
		b->busy = FALSE;
	}
	if (b->valueP)
		*b->valueP = arg;
}


long wIntegerGetValue(
		wInteger_p b )
{
	long ret;
	const char * cp;
	if (b->widget == 0) abort();
	cp = gtk_entry_get_text( GTK_ENTRY(b->widget) );
	ret = atol( cp );
	return ret;
}


static void triggerInteger(
		wControl_p b )
{
	wInteger_p bi = (wInteger_p)b;
	const char * s;
	char * cp;
	long v;

	if (b == 0)
		return;
	if (bi->widget == 0) abort();
	s = gtk_entry_get_text( GTK_ENTRY(bi->widget) );
	if (debugWindow >= 2) printf("%s text = %s\n", bi->labelStr?bi->labelStr:"No label", s );
	if (s == NULL) 
		return;
	v = strtol( s, &cp, 10 );
	if (*cp!=0 || v < bi->low || v > bi->high)
		return;
	/*if (bi->oldValue == v)
		return;*/
	if (debugWindow >= 2) printf("triggerInteger( %ld )\n", v );
	bi->oldValue = v;
	if (bi->valueP)
		*bi->valueP = v;
	if (bi->action) {
		bi->busy = TRUE;
		bi->action( v, bi->data );
		bi->busy = FALSE;
	}
	gtkSetTrigger( NULL, NULL );
	return;
}



static void integerActivated(
		GtkEntry *widget,
		wInteger_p b )
{
	const char * s;
	char * cp;
	long v;
	char val_s[80];

	if (b == 0)
		return;
	if (b->widget == 0) abort();
	s = gtk_entry_get_text( GTK_ENTRY(b->widget) );
	if (debugWindow >= 2) printf("%s text = %s\n", b->labelStr?b->labelStr:"No label", s );
	if (s != NULL) {
		v = strtod( s, &cp );
		if (*cp != '\n' && *cp != '\0') {
			wNoticeEx( NT_ERROR, "The value you have entered is not a valid number\nPlease try again", "Ok", NULL );
		} else if (v < b->low || v > b->high) {
			sprintf( val_s, "Please enter a value between %ld and %ld", b->low, b->high );
			wNoticeEx( NT_ERROR, val_s, "Ok", NULL );
		} else {
			if (debugWindow >= 2) printf("integerActivated( %ld )\n", v );
			b->oldValue = v;
			if (b->valueP)
				*b->valueP = v;
			if (b->action) {
				gtkSetTrigger( NULL, NULL );
				b->busy = TRUE;
				b->action( v, b->data );
				b->busy = FALSE;
			}
			return;
		}
		sprintf( val_s, "%ld", b->oldValue);
		b->busy = TRUE;
		gtk_entry_set_text( GTK_ENTRY(b->widget), val_s );
		b->busy = FALSE;
	}
	return;
}

static void integerChanged(
		GtkEntry *widget,
		wInteger_p b )
{
	const char * s;
	char * cp;
	long v;

	if (b == 0)
		return;
	if (b->widget == 0) abort();
	if (b->busy)
		return;
	s = gtk_entry_get_text( GTK_ENTRY(b->widget) );
	if (s == NULL)
		return;
	if (debugWindow >= 2) printf("%s text = %s\n", b->labelStr?b->labelStr:"No label", s );
	if ( s[0] == '\0' ||
		 strcmp( s, "-" ) == 0 ) {
		v = 0;
	} else {
		v = strtol( s, &cp, 10 );
		if (*cp != '\0'
#ifdef LATER
				 || v < b->low || v > b->high
#endif
						) {
			wBeep();
			wIntegerSetValue( b, b->oldValue );
			return;
		}
	}
	b->oldValue = v;
	if (b->valueP != NULL) {
		*b->valueP = v;
	}
	if (b->action)
		gtkSetTrigger( (wControl_p)b, triggerInteger );
	return;
}

wInteger_p wIntegerCreate(
		wWin_p	parent,
		wPos_t	x,
		wPos_t	y,
		const char 	* helpStr,
		const char	* labelStr,
		long	option,
		wPos_t	width,
		long	low,
		long	high,
		long	*valueP,
		wIntegerCallBack_p action,
		void 	*data )
{
	wInteger_p b;

	b = (wInteger_p)gtkAlloc( parent, B_TEXT, x, y, labelStr, sizeof *b, data );
	b->valueP =  valueP;
	b->action = action;
	b->option = option;
	b->low = low;
	b->high = high;
	gtkComputePos( (wControl_p)b );

	b->widget = (GtkWidget*)gtk_entry_new_with_max_length( 20 );
	if (b->widget == 0) abort();

#ifndef GTK1
	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), b->widget );
	gtk_widget_set_uposition( b->widget, b->realX, b->realY );
#endif
	if ( width )
#ifndef GTK1
		gtk_widget_set_size_request( b->widget, width, -1 );
#else
		gtk_widget_set_usize( b->widget, width, -1 );
#endif
	gtkControlGetSize( (wControl_p)b );
	if (labelStr)
		b->labelW = gtkAddLabel( (wControl_p)b, labelStr );
	/*b->w += 4;*/
	/*b->h += 4;*/
	if (b->valueP)
		wIntegerSetValue( b, *b->valueP );
	else
		wIntegerSetValue( b, b->low>0?b->low:0.0 );
	gtk_widget_show( b->widget );
	gtkAddButton( (wControl_p)b );
	gtkAddHelpString( b->widget, helpStr );
	gtk_signal_connect( GTK_OBJECT(b->widget), "changed", GTK_SIGNAL_FUNC(integerChanged), b );
	gtk_signal_connect( GTK_OBJECT(b->widget), "activate", GTK_SIGNAL_FUNC(integerActivated), b );
	if (option & BO_READONLY)
		gtk_entry_set_editable( GTK_ENTRY(b->widget), FALSE );
	return b;
}
