/** \file gtktext.c
 * Multi-line Text Boxes
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

#define GTK_ENABLE_BROKEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "i18n.h"
#include "gtkint.h"

/*
 * Disable USE_TEXTVIEW to use the deprecated gtk_text
 */
#define USE_TEXTVIEW


struct PrintData {
	wText_p	tb;
	gint lines_per_page;
	gdouble font_size; 
	gchar **lines;
	gint total_lines;
	gint total_pages;
};

#define HEADER_HEIGHT 20.0
#define HEADER_GAP 8.5

struct wText_t {
		WOBJ_COMMON
		wPos_t width, height;
		int changed;
		GtkWidget * text;
		GtkWidget * vscroll;
		};

EXPORT void wTextClear(
		wText_p bt )
{
#ifdef USE_TEXTVIEW
	GtkTextBuffer * tb;
#endif
	if (bt->text == 0) abort();
#ifdef USE_TEXTVIEW
	tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(bt->text) );
	gtk_text_buffer_set_text( tb, "", -1 );
	if (bt->option & BO_READONLY)
		gtk_text_view_set_editable( GTK_TEXT_VIEW(bt->text), FALSE );
#else
	gtk_text_set_point( GTK_TEXT(bt->text), 0 );
	gtk_text_forward_delete( GTK_TEXT(bt->text), gtk_text_get_length( GTK_TEXT(bt->text) ) );
	if (bt->option & BO_READONLY)
		gtk_text_set_editable( GTK_TEXT(bt->text), FALSE );
#endif
	bt->changed = FALSE;
}

EXPORT void wTextAppend(
		wText_p bt,
		const char * text )
{
#ifdef USE_TEXTVIEW
	GtkTextBuffer * tb;
	GtkTextIter ti1, ti2;
#else
	static GdkFont * fixedRegularFont = NULL;
	static GdkFont * fixedBoldFont = NULL;
	static GdkFont * variableRegularFont = NULL;
	static GdkFont * variableBoldFont = NULL;
	GdkFont * regularFont = NULL;
	GdkFont * boldFont = NULL;
#endif
	wBool_t doBold;
	char * cp;
	int len;

	if (bt->text == 0) abort();
#ifdef USE_TEXTVIEW
	tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(bt->text) );
#else
	if ((bt->option&BT_FIXEDFONT)) {
		if (fixedRegularFont==NULL)
			fixedRegularFont = gdk_font_load( "-*-courier-medium-r-*-*-12-*-*-*-*-*-iso8859-*" );
		if (fixedBoldFont==NULL)
			fixedBoldFont = gdk_font_load( "-*-courier-bold-r-*-*-12-*-*-*-*-*-iso8859-*" );
		regularFont = fixedRegularFont;
		boldFont = fixedBoldFont;
	} else {
		if (variableRegularFont==NULL)
			variableRegularFont = gdk_font_load( "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-iso8859-*" );
		if (variableBoldFont==NULL)
			variableBoldFont = gdk_font_load( "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-iso8859-*" );
		regularFont = variableRegularFont;
		boldFont = variableBoldFont;
	}
#endif
	/*gtk_text_freeze( GTK_TEXT (bt->text) );*/
	doBold = FALSE;
	text = gtkConvertInput( text );
	while ( text && *text ) {
		if ( (bt->option & BT_DOBOLD) != 0 &&
			 ( cp = strchr( text, doBold?'>':'<' ) ) != NULL ) {
			len = cp-text;
			cp++;
		} else {
			len = -1;
			cp = NULL;
		}
		if ( len != 0 ) {
#ifdef USE_TEXTVIEW
			gtk_text_buffer_get_bounds( tb, &ti1, &ti2 );
			if ( !doBold )
				gtk_text_buffer_insert( tb, &ti2, text, len );
			else
				gtk_text_buffer_insert_with_tags_by_name( tb, &ti2, text, len, "bold", NULL );
#else
			gtk_text_insert( GTK_TEXT(bt->text), doBold?boldFont:regularFont, &bt->text->style->black, NULL, text, len );
#endif
		}
		text = cp;
		doBold = !doBold;
	}
	/*gtk_text_set_point( GTK_TEXT(bt->text), gtk_text_get_length( GTK_TEXT(bt->text) )-1 );*/
	/*gtk_text_thaw( GTK_TEXT (bt->text) );*/
	bt->changed = FALSE;
}

EXPORT void gtkTextFreeze(
		wText_p bt )
{
#ifdef USE_TEXTVIEW
	gtk_text_view_set_editable( GTK_TEXT_VIEW(bt->text), FALSE );
#else
	gtk_text_freeze( GTK_TEXT (bt->text) );
#endif
}

EXPORT void gtkTextThaw(
		wText_p bt )
{
#ifdef USE_TEXTVIEW
	gtk_text_view_set_editable( GTK_TEXT_VIEW(bt->text), TRUE );
#else
	gtk_text_thaw( GTK_TEXT (bt->text) );
#endif
}

EXPORT void wTextReadFile(
		wText_p bt,
		const char * fileName )
{
	FILE * f;
	char buff[BUFSIZ+1];
	if (fileName) {
		f = fopen( fileName, "r" );
		if (f == NULL) {
			perror( fileName );
			return;
		}
		while (fgets( buff, sizeof buff, f ) != NULL ) {
			wTextAppend( bt, buff );
		}
	}
}


#ifdef USE_TEXTVIEW
static char * gtkGetText(
		wText_p bt )
{
	GtkTextBuffer * tb;
	GtkTextIter ti1, ti2;
	char * cp;
	if (bt->text == 0) abort();
	tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(bt->text) );
	gtk_text_buffer_get_bounds( tb, &ti1, &ti2 );
	cp = gtk_text_buffer_get_text( tb, &ti1, &ti2, FALSE );
	cp = gtkConvertOutput( cp );
	return cp;
}
#endif


EXPORT wBool_t wTextSave(
		wText_p bt,
		const char * fileName )
{
#ifndef USE_TEXTVIEW
	int siz, pos, cnt;
#endif
	FILE * f;
	char * cp;

	f = fopen( fileName, "w" );
	if (f==NULL) {
		wNoticeEx( NT_ERROR, fileName, "Ok", NULL );
		return FALSE;
	}
#ifdef USE_TEXTVIEW
	cp = gtkGetText( bt );
	fwrite( cp, 1, strlen(cp), f );
	free(cp);
#else
	siz = gtk_text_get_length( GTK_TEXT(bt->text) );
	pos = 0;
	cnt = BUFSIZ;
	while (pos<siz) {
		if (pos+cnt>siz)
			 cnt = siz-pos;
		cp = gtk_editable_get_chars( GTK_EDITABLE(bt->text), pos, pos+cnt );
		if (cp == NULL)
			break;
		fwrite( cp, 1, cnt, f );
		free(cp);
		pos += cnt;
	}
#endif
	fclose(f);
	return TRUE;
}
/**
 * Begin the printing by retrieving the contents of the text box and
 * count the lines of text. 
 * 
 * \param operation IN the GTK print operation
 * \param context IN print context
 * \param pd IN data structure for user data
 * 
 */
 
static void
begin_print (GtkPrintOperation *operation, 
             GtkPrintContext *context,
             struct PrintData *pd)
{
	gchar *contents;
	gdouble height;
		
	contents =  gtkGetText( pd->tb );
	pd->lines = g_strsplit (contents, "\n", 0);

	/* Count the total number of lines in the file. */
	/* ignore the header lines */
	pd->total_lines = 6;
	while (pd->lines[pd->total_lines] != NULL)
		pd->total_lines++;
  
	/* Based on the height of the page and font size, calculate how many lines can be 
	* rendered on a single page. A padding of 3 is placed between lines as well.
	* Space for page header, table header and footer lines is subtracted from the total size
	*/
	height = gtk_print_context_get_height (context) - (pd->font_size + 3) - 2 * ( HEADER_HEIGHT + HEADER_GAP );
	pd->lines_per_page = floor (height / (pd->font_size + 3));
	pd->total_pages = (pd->total_lines - 1) / pd->lines_per_page + 1;
	gtk_print_operation_set_n_pages (operation, pd->total_pages);
	
	free( contents );
}

/**
 * Draw the page, which includes a header with the file name and page number along
 * with one page of text with a font of "Monospace 10". 
 * 
 * \param operation IN the GTK print operation
 * \param context IN print context
 * \param page_nr IN page to print
 * \param pd IN data structure for user data
 * 
 * 
 */
 
static void
draw_page (GtkPrintOperation *operation,
           GtkPrintContext *context,
           gint page_nr,
           struct PrintData *pd )
{
	cairo_t *cr;
	PangoLayout *layout;
	gdouble width, text_height, height;
	gint line, i, text_width, layout_height;
	PangoFontDescription *desc;
	gchar *page_str;

	cr = gtk_print_context_get_cairo_context (context);
	width = gtk_print_context_get_width (context);
	
	layout = gtk_print_context_create_pango_layout (context);
	desc = pango_font_description_from_string ("Monospace");
	pango_font_description_set_size (desc, pd->font_size * PANGO_SCALE);

	/* 
	 * render the header line with document type parts list on left and
	 * first line of layout title on right 
	 */ 
	 
	pango_layout_set_font_description (layout, desc);
	pango_layout_set_text (layout, pd->lines[ 0 ], -1); 	// document type
	pango_layout_set_width (layout, -1);
	pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
	pango_layout_get_size (layout, NULL, &layout_height);
	text_height = (gdouble) layout_height / PANGO_SCALE;

	cairo_move_to (cr, 0, (HEADER_HEIGHT - text_height) / 2);
	pango_cairo_show_layout (cr, layout);

	pango_layout_set_text (layout, pd->lines[ 2 ], -1);		// layout title 
	pango_layout_get_size (layout, &text_width, NULL);
	pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

	cairo_move_to (cr, width - (text_width / PANGO_SCALE), 
                 (HEADER_HEIGHT - text_height) / 2);
	pango_cairo_show_layout (cr, layout);

	/* Render the column header */
	cairo_move_to (cr, 0, HEADER_HEIGHT + HEADER_GAP + pd->font_size + 3 );
	pango_layout_set_text (layout, pd->lines[ 6 ], -1);
	pango_cairo_show_layout (cr, layout);
	cairo_rel_move_to (cr, 0, pd->font_size + 3 );
	pango_layout_set_text (layout, pd->lines[ 7 ], -1);
	pango_cairo_show_layout (cr, layout);
	
	/* Render the page text with the specified font and size. */  
	cairo_rel_move_to (cr, 0, pd->font_size + 3 );
	line = page_nr * pd->lines_per_page + 8;
	for (i = 0; i < pd->lines_per_page && line < pd->total_lines; i++) 
	{
		pango_layout_set_text (layout, pd->lines[line], -1);
		pango_cairo_show_layout (cr, layout);
		cairo_rel_move_to (cr, 0, pd->font_size + 3);
		line++;
	}

	/* 
	 * Render the footer line with date on the left and page number 
	 * on the right
	 */
	pango_layout_set_text (layout, pd->lines[ 5 ], -1); 	// date
	pango_layout_set_width (layout, -1);
	pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
	pango_layout_get_size (layout, NULL, &layout_height);
	text_height = (gdouble) layout_height / PANGO_SCALE;

	height = gtk_print_context_get_height (context);
	cairo_move_to (cr, 0, height - ((HEADER_HEIGHT - text_height) / 2));
	pango_cairo_show_layout (cr, layout);

	page_str = g_strdup_printf (_("%d of %d"), page_nr + 1, pd->total_pages); // page number
	pango_layout_set_text( layout, page_str, -1 );
	pango_layout_get_size (layout, &text_width, NULL);
	pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

	cairo_move_to (cr, width - (text_width / PANGO_SCALE), 
                 height - ((HEADER_HEIGHT - text_height) / 2));
	pango_cairo_show_layout (cr, layout);

	g_free (page_str);
	g_object_unref (layout);
	pango_font_description_free (desc);
}

/**
 * Clean up after the printing operation since it is done. 
 *
 * \param operation IN the GTK print operation
 * \param context IN print context
 * \param pd IN data structure for user data
 * 
 * 
 */
static void
end_print (GtkPrintOperation *operation, 
           GtkPrintContext *context,
           struct PrintData *pd)
{
	g_strfreev (pd->lines);
	free( pd );
}

/**
 * Print the content of a multi line text box. This function is only used
 * for printing the parts list. So it makes some assumptions on the structure
 * and the content. Change if the multi line entry is changed.
 * The deprecated gtk_text is not supported by this function. 
 * 
 * Thanks to Andrew Krause's book for a good starting point.
 *
 * \param bt IN the text field
 * \return    TRUE on success, FALSE on error
 */

EXPORT wBool_t wTextPrint(
		wText_p bt )
{
	GtkPrintOperation *operation;
	GtkWidget *dialog;
	GError *error = NULL;
	gint res;
	struct PrintData *data;

	/* Create a new print operation, applying saved print settings if they exist. */
	operation = gtk_print_operation_new ();
	WlibApplySettings( operation );
  
	data = malloc(sizeof( struct PrintData));
	data->font_size = 10.0;
	data->tb = bt;

	g_signal_connect (G_OBJECT (operation), "begin_print", 
		                G_CALLBACK (begin_print), (gpointer) data);
	g_signal_connect (G_OBJECT (operation), "draw_page", 
		                G_CALLBACK (draw_page), (gpointer) data);
	g_signal_connect (G_OBJECT (operation), "end_print", 
		                G_CALLBACK (end_print), (gpointer) data);

	/* Run the default print operation that will print the selected file. */
	res = gtk_print_operation_run (operation, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, 
									GTK_WINDOW(gtkMainW->gtkwin), &error);

	/* If the print operation was accepted, save the new print settings. */
	if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
	{
		WlibSaveSettings( operation );
	}
	/* Otherwise, report that the print operation has failed. */
	else if (error)
	{
		dialog = gtk_message_dialog_new (GTK_WINDOW (gtkMainW->gtkwin), 
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				                             "%s", error->message);
    
		g_error_free (error);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);     
	}
	g_object_unref (operation);
  
	return TRUE;
}


EXPORT int wTextGetSize(
		wText_p bt )
{
#ifdef USE_TEXTVIEW
	char * cp = gtkGetText( bt );
	int len = strlen( cp );
	free( cp );
	return len;
#else
	return (int)gtk_text_get_length( GTK_TEXT(bt->text) );
#endif
}


EXPORT void wTextGetText(
		wText_p bt,
		char * text,
		int len )
{
	char * cp;
#ifdef USE_TEXTVIEW
	cp = gtkGetText(bt);
	strncpy( text, cp, len );
	free( cp );
#else
	cp = gtk_editable_get_chars( GTK_EDITABLE(bt->text), 0, len );
	strncpy( text, cp, len );
#endif
}


EXPORT void wTextSetReadonly (
		wText_p bt,
		wBool_t ro )
{
#ifdef USE_TEXTVIEW
	gtk_text_view_set_editable( GTK_TEXT_VIEW(bt->text), !ro );
#else
	gtk_text_set_editable( GTK_TEXT(bt->text), !ro );
#endif
	if (ro) {
		bt->option |= BO_READONLY;
	} else {
		bt->option &= ~BO_READONLY;
	}
}


EXPORT wBool_t wTextGetModified(
		wText_p bt )
{
	return bt->changed;
}


EXPORT void wTextSetSize( 
		wText_p bt,
		wPos_t w,
		wPos_t h )
{
#ifdef USE_TEXTVIEW
	gtk_widget_set_size_request( bt->widget, w, h );
//	gtk_widget_set_size_request( bt->text, w-15, h );
//	gtk_widget_set_size_request( bt->vscroll, 15, h );
#else
	gtk_widget_set_usize( bt->widget, w, h );
	gtk_widget_set_usize( bt->text, w-15, h );
	gtk_widget_set_usize( bt->vscroll, 15, h );
	gtk_widget_queue_resize( GTK_WIDGET(bt->widget) );
	gtk_widget_queue_resize( GTK_WIDGET(bt->text) );
	gtk_widget_queue_resize( GTK_WIDGET(bt->vscroll) );
#endif
	bt->w = w;
	bt->h = h;
}


EXPORT void wTextComputeSize(
		wText_p bt,
		int rows,
		int cols,
		wPos_t *width,
		wPos_t *height )
{
	*width = rows * 7;
	*height = cols * 14;
}


EXPORT void wTextSetPosition(
		wText_p bt,
		int pos )
{
#ifdef USE_TEXTVIEW
	/* TODO */
#else
	GTK_TEXT(bt->text)->first_line_start_index = pos;
	gtk_text_set_word_wrap( GTK_TEXT(bt->text), TRUE );
	gtk_text_set_point( GTK_TEXT(bt->text), pos );
#endif
}

static void textChanged(
		GtkWidget * widget,
		wText_p bt )
{
	if (bt == 0)
		return;
	bt->changed = TRUE;
}


EXPORT wText_p wTextCreate(
		wWin_p	parent,
		wPos_t	x,
		wPos_t	y,
		const char 	* helpStr,
		const char	* labelStr,
		long	option,
		wPos_t	width,
		wPos_t	height )
{
	wText_p bt;
#ifdef USE_TEXTVIEW
	GtkTextBuffer * tb;
#else
	GtkRequisition requisition;
#endif
	bt = gtkAlloc( parent, B_MULTITEXT, x, y, labelStr, sizeof *bt, NULL );
	gtkComputePos( (wControl_p)bt );
	bt->width = width;
	bt->height = height;
	bt->option = option;
	gtkComputePos( (wControl_p)bt );

#ifdef USE_TEXTVIEW
	bt->widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (bt->widget),
		   	           GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);
	bt->text = gtk_text_view_new();
	if (bt->text == 0) abort();
	gtk_container_add (GTK_CONTAINER (bt->widget), bt->text);
	tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(bt->text) );
	gtk_text_buffer_create_tag( tb, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
/*	gtk_text_buffer_create_tag( tb, "italic", "style", PANGO_STYLE_ITALIC, NULL); */
/*	gtk_text_buffer_create_tag( tb, "bolditalic", "weight", PANGO_WEIGHT_BOLD, "style", PANGO_STYLE_ITALIC, NULL); */
	bt->vscroll = gtk_vscrollbar_new( GTK_TEXT_VIEW(bt->text)->vadjustment );
	if (bt->vscroll == 0) abort();
#else
	bt->widget = gtk_hbox_new( FALSE, 0 );
	bt->text = gtk_text_new( NULL, NULL );
	if (bt->text == 0) abort();
	gtk_box_pack_start( GTK_BOX(bt->widget), bt->text, FALSE, FALSE, 0 );
	bt->vscroll = gtk_vscrollbar_new( GTK_TEXT(bt->text)->vadj );
	if (bt->vscroll == 0) abort();
	gtk_box_pack_start( GTK_BOX(bt->widget), bt->vscroll, FALSE, FALSE, 0 );
#endif
	if (option&BT_CHARUNITS) {
		width *= 7;
		height *= 14;
	}
	gtk_widget_show( bt->text );
	gtk_widget_show( bt->vscroll );
	gtk_widget_show( bt->widget );
#ifdef USE_TEXTVIEW
//	gtk_widget_set_size_request( GTK_WIDGET(bt->text), width, height );
//	gtk_widget_set_size_request( GTK_WIDGET(bt->vscroll), -1, height );
	gtk_widget_set_size_request( GTK_WIDGET(bt->widget), width+15/*requisition.width*/, height );
#else
	gtk_widget_set_usize( GTK_WIDGET(bt->text), width, height );
	gtk_widget_set_usize( GTK_WIDGET(bt->vscroll), -1, height );
	gtk_widget_size_request( GTK_WIDGET(bt->vscroll), &requisition );
	gtk_widget_set_usize( GTK_WIDGET(bt->widget), width+15/*requisition.width*/, height );
#endif
	if (bt->option&BO_READONLY) {
#ifdef USE_TEXTVIEW
		gtk_text_view_set_editable( GTK_TEXT_VIEW(bt->text), FALSE );
		gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(bt->text), FALSE);
#else
		gtk_text_set_editable( GTK_TEXT(bt->text), FALSE );
#endif
	}

#ifdef USE_TEXTVIEW
	gtk_fixed_put( GTK_FIXED(parent->widget), bt->widget, bt->realX, bt->realY );
#else
	gtk_container_add( GTK_CONTAINER(parent->widget), bt->widget );
	gtk_widget_set_uposition( bt->widget, bt->realX, bt->realY );
#endif
	gtkControlGetSize( (wControl_p)bt );
	if (labelStr)
		bt->labelW = gtkAddLabel( (wControl_p)bt, labelStr );
#ifdef USE_TEXTVIEW
	gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW(bt->text), GTK_WRAP_WORD );
#else
	gtk_text_set_word_wrap( GTK_TEXT(bt->text), TRUE );
#endif
	gtk_widget_realize( bt->text );
	gtkAddButton( (wControl_p)bt );
	gtkAddHelpString( bt->widget, helpStr );
#ifdef USE_TEXTVIEW
	g_signal_connect( G_OBJECT(tb), "changed", GTK_SIGNAL_FUNC(textChanged), bt );
#else
	gtk_signal_connect( GTK_OBJECT(bt->text), "changed", GTK_SIGNAL_FUNC(textChanged), bt );
#endif
	return bt;
}
