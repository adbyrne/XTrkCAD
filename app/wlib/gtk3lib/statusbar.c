/** \file statusbar.c
 * Status bar
 */

/* 	XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis,
 *                2017 Martin Fischer <m_fischer@sf.net>
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

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

struct wStatus_t {
    GtkWidget * labelWidget;
};

/**
 * Set the message text
 *
 * \param b IN widget
 * \param arg IN new text
 * \return
 */

void wStatusSetValue(
    wControl_p b,
    const char * arg)
{
    if (!b || b->widget == 0) {
        return;
    }

    gtk_label_set_text(GTK_LABEL(b->widget), wlibConvertInput(arg));
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

wControl_p wStatusCreate(
        wControl_p	parent,
        const char 	* labelStr,
        const char	*message)
{
	wControl_p b;

	b = wlibControlNew(B_STATUS, parent, NULL, NULL);

    b->widget = wlibWidgetFromIdWarn(parent, labelStr);

	gtk_label_set_text(GTK_LABEL(b->widget),
		                   message?wlibConvertInput(message):"");

	gtk_widget_show(b->widget);
	return b;
}

/**
 * Get the anticipated length of a message field
 *
 * \param testString IN string that should fit into the message box
 * \return expected width of message box
 */

wWinPix_t
wStatusGetWidth(const char *testString)
{
    printf("Function at line %d in %s is not implemented!", __LINE__, __FILE__);
    return(0);
 //   GtkWidget *entry;
 //   GtkRequisition min_req, nat_req;

 //   entry = gtk_entry_new();
 //   g_object_ref_sink(entry);

 //   gtk_entry_set_has_frame(GTK_ENTRY(entry), FALSE);
 //   gtk_entry_set_width_chars(GTK_ENTRY(entry), strlen(testString));
 //   gtk_entry_set_max_length(GTK_ENTRY(entry), strlen(testString));

 ////   gtk_widget_get_preferred_size(entry, NULL, &requisition);
 //   gtk_widget_get_preferred_size(entry, &min_req, &nat_req);
 //   gtk_widget_destroy(entry);
 //   g_object_unref(entry);

 //   return (nat_req.width);
}

/**
 * Get height of message text
 *
 * \param flags IN text properties (large or small size)
 * \return text height
 */
static int fonts_set = 0;

wWinPix_t wStatusGetHeight(
    long flags)
{
    printf("Function at line %d in %s is not implemented!", __LINE__, __FILE__);
    return(0);
  //  GtkWidget * temp;

  //  if (!(flags&COMBOBOX)) {
		//temp = gtk_entry_new();	 //To get size of text itself
  //      gtk_entry_set_has_frame(GTK_ENTRY(temp), FALSE);
  //  } else {
  //      temp = gtk_combo_box_text_new();    //to get max size of an object in infoBar
  //  }
  //  g_object_ref_sink(temp);

  //  if (wMessageSetFont(flags))	{
		//if (!fonts_set) {
		//	GtkStyleContext *context;
		//	GtkCssProvider *smallProvider = gtk_css_provider_new();
		//	GtkCssProvider *largeProvider = gtk_css_provider_new();
		//	/* get the current font descriptor */
		//	context = gtk_widget_get_style_context(temp);
		//	static const char smallStyle[] = " .smallLabel { font-size: 70% } ";

		//	gtk_css_provider_load_from_data (smallProvider,
		//	                                 smallStyle, -1, NULL);
		//	gtk_style_context_add_provider(context,
		//	                               GTK_STYLE_PROVIDER(smallProvider),
		//	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

		//	static const char largeStyle[] = " .largeLabel{ font-size: 140% }  ";

		//	gtk_css_provider_load_from_data (largeProvider,
		//	                                 largeStyle, -1, NULL);
		//	gtk_style_context_add_provider(context,
		//	                               GTK_STYLE_PROVIDER(largeProvider),
		//	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


		//	fonts_set = 1;

		//}

		///* set the new font size */
		//GtkStyleContext * context = gtk_widget_get_style_context(GTK_WIDGET(temp));
		//if (flags & BM_LARGE) {
		//	gtk_style_context_add_class(context, "largeLabel");
		//} else {
		//	gtk_style_context_add_class(context, "smallLabel");
		//}
  //  }

  //  if (flags&1L) {
  //      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(temp),"Test");
  //  }

  //  GtkRequisition temp_requisition;
  //   gtk_widget_get_preferred_size(temp, NULL, &temp_requisition);
  //  //g_object_ref_sink(temp);
  //  //g_object_unref(temp);
  //  gtk_widget_destroy(temp);
  //  return temp_requisition.height;
}

/**
 * Set the width of the widget
 *
 * \param b IN widget
 * \param width IN  new width
 * \return
 */

void wStatusSetWidth(
    wControl_p b,
    wWinPix_t width)
{
	printf("Function at line %d in %s is not implemented!", __LINE__, __FILE__);
	return;

 //   b->labelWidth = width;
 //   gtk_widget_set_size_request(b->labelWidget, width, -1);
}
