#include "wlib.h"
/** \file fileselector.c
 * Create and handle file selectors
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
#include <glib-object.h>
#include "gtkint.h"
#include "i18n.h"

struct wFilSel_t {
	wType_e type;                       /**< */
	GtkFileChooserNative * window; 		/**<  file selector handle*/
	wFilSelCallBack_p action; 			/**<  */
	void * attributes;
	GPtrArray* filters;					/**<  file type filters */
	wFilSelMode_e mode;					/**< used for load or save */
	int opt; 							/**< see FSO_ options */
	const char * title; 				/**< dialog box title */
	wWindow_p parent; 					/**< parent window */
	char *defaultExtension; 			/**< to use if no extension specified */
};

/**
 * Create filters from pattern string. The pattern string consists of pairs
 * of filter definitions. First is the name of the filter, second the select
 * pattern. The parts are delemited by a vertical bar.
 * Eg: "Text Files|*.txt|"
 *
 * \param fileSelect	file selector widget
 * \param filterPattern	the pattern string
 */

static void
CreateFilters(struct wFilSel_t* fileSelect, const char* filterPattern)
{
	fileSelect->filters = g_ptr_array_new();

	if (fileSelect->opt & FSO_PICTURES) {
		GtkFileFilter* filter = gtk_file_filter_new();
		gtk_file_filter_set_name(filter, "Supported Image Formats");
		gtk_file_filter_add_pixbuf_formats(filter);
		g_ptr_array_add(fileSelect->filters, (gpointer)filter);

		filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, "*");
		gtk_file_filter_set_name(filter, "All files");

		g_ptr_array_add(fileSelect->filters, (gpointer)filter);
	} else {
		char* patternList = g_strdup(filterPattern);
		char* name = strtok(patternList, "|");

		while (name) {
			char* pattern;
			GtkFileFilter* filter = gtk_file_filter_new();

			gtk_file_filter_set_name(filter, name);
			pattern = strtok(NULL, "|");
			gtk_file_filter_add_pattern(filter, pattern);
			g_ptr_array_add(fileSelect->filters, (gpointer)filter);
			name = strtok(NULL, "|");
		}
		g_free(patternList);
	}
}

/**
 * Add the already created filters to the dialog.
 *
 * \param selector
 */

static void
AddFilters(struct wFilSel_t* selector)
{
	for (unsigned int i = 0; i < selector->filters->len; i++) {
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(selector->window),
		                            GTK_FILE_FILTER(g_ptr_array_index(selector->filters, i)));
	}
}

/**
 * Create a new file selector. Only the internal attributes structures are
 * set up, no dialog is created.
 *
 * If the FSO_PICTURES flag is set in opt , the pattern are automatically
 * created from the supported image formats. Any pattern passed in the
 * pattlist are ignored in that case.
 * After executing the file selector, the action callback is executed. The
 * callback receives three arguments:
 * - int files number of selected files (1 or more if FS_MULTIPLEFILES)
 * - char ** names an array of files filenames
 * - void * attributes	the attributes passed to wFilSelCreate()
 *
 * \param w		IN	parent window
 * \param mode	IN	load or save
 * \param opt	IN	?
 * \param title IN	dialog title
 * \param pattList IN list of filter patterns
 * \param action IN callback
 * \param attributes IN	see description
 * \return    the newly created file selector structure
 */

struct wFilSel_t * wFilSelCreate(
        wControl_p w,
        wFilSelMode_e mode,
        int opt,
        const char * title,
        const char * pattList,
        wFilSelCallBack_p action,
        void * attributes )
{
	struct wFilSel_t	*fs;

	fs = (struct wFilSel_t*)g_malloc0(sizeof *fs);

	fs->type = W_FILESELECT;
	fs->parent = w;
	fs->window = 0;
	fs->mode = mode;
	fs->opt = opt;
	fs->title = g_strdup( title );
	fs->action = action;
	fs->attributes = attributes;

	if (pattList || (opt & FSO_PICTURES)) {
		CreateFilters(fs, pattList);
	}
	return fs;
}

/**
 * Show and handle the file selection dialog.
 *
 * \param fs IN file selection
 * \param dirName IN starting directory
 * \return    always TRUE
 */

int wFilSelect( struct wFilSel_t * fs, const char * dirName )
{

	g_assert(fs->type == W_FILESELECT);

	if (fs->window == NULL) {

		fs->window = gtk_file_chooser_native_new(fs->title,
		             NULL,
		             (fs->mode == FS_LOAD ? GTK_FILE_CHOOSER_ACTION_OPEN :
		              GTK_FILE_CHOOSER_ACTION_SAVE),
		             (fs->mode == FS_LOAD ? ("Open") : ("Save")),
		             ("Cancel"));

		if (fs->window==0) { abort(); }

		if (fs->mode == FS_SAVE) {
			// get confirmation before overwriting an existing file
			gtk_file_chooser_set_do_overwrite_confirmation(
			        GTK_FILE_CHOOSER(fs->window), true);
		}

		if (fs->opt & FSO_MULTIPLEFILES) {
			gtk_file_chooser_set_select_multiple(
			        GTK_FILE_CHOOSER(fs->window), true);
		}

		if (fs->filters) {
			AddFilters(fs);
		}

		// Add possible shortcut to the demo folder
		// gtk_file_chooser_add_shortcut_folder(fs->window, "\\XtrackCAD\\share\\demos", NULL);

	}

	gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(fs->window), dirName );

	int resp = gtk_native_dialog_run( GTK_NATIVE_DIALOG( fs->window ));

	if( resp == GTK_RESPONSE_ACCEPT || resp == GTK_RESPONSE_APPLY) {
		char **fileNames;
		GSList *fileNameList;

		fileNameList = gtk_file_chooser_get_uris( GTK_FILE_CHOOSER(fs->window) );
		fileNames = g_malloc0( sizeof(char *) * g_slist_length (fileNameList) );

		for (unsigned i=0; i < g_slist_length (fileNameList); i++ ) {
			char* host;
			char* file;
			GError* err = NULL;

			file = g_filename_from_uri( g_slist_nth_data( fileNameList, i ),
			                            &host, &err );

			fileNames[ i ] = file;
			g_free( g_slist_nth_data ( fileNameList, i));
		}

		if (fs->action) {
			fs->action( g_slist_length(fileNameList), fileNames, fs->attributes );
		}

		for(unsigned i=0; i < g_slist_length(fileNameList); i++) {
			g_free( fileNames[ i ]);
		}
		g_free( fileNames );
		g_slist_free (fileNameList);
	}

	return true;
}
