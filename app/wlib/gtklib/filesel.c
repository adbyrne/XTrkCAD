/** \file filesel.c
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

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <unistd.h>
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <glib-object.h>
#include "gtkint.h"
#include "i18n.h"

#define MAX_ALLOWEDFILTERS 10

struct wFilSel_t {
		GtkWidget * window; 							/**<  file selector handle*/
		wFilSelCallBack_p action; 						/**<  */
		void * data; 									/**<  */
		int pattCount; 									/**<  number of file patterns*/
		wBool_t loadPatternsAdded;						/** Already loaded        	*/
		GtkFileFilter *filter[ MAX_ALLOWEDFILTERS ]; 	/**< array of file patterns */
		wFilSelMode_e mode; 							/**< used for load or save */
		int opt; 										/**< see FS_ options */
		const char * title; 							/**< dialog box title */
		wWin_p parent; 									/**< parent window */
		char *defaultExtension; 						/**< to use if no extension specified */
		};

/**
 * Signal handler for 'changed' signal of custom combo box. The filter
 * is set accordinng to the file format active in the combo box
 * 
 * \param comboBox the combo box 
 * \param fileSelector data of the file selector
 * 
 */

static void FileFormatChanged( GtkWidget *comboBox, 
						  struct wFilSel_t *fileSelector )
{
	// get active entry
	int entry = (int)gtk_combo_box_get_active (GTK_COMBO_BOX(comboBox));
	
	if( entry>=0 ) {
		g_object_ref(G_OBJECT( (fileSelector->filter)[ entry ])); 
		gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(fileSelector->window ),						
									(fileSelector->filter)[ entry ]);
	}
}

/**
 * Create a widget containing a combo box for selecting a file format. 
 * From an array of filters, the names are retrieved and used to populate
 * the combo box. 
 * \param IN dialogBox
 * \param patterns IN number of entries for combo
 * \param filters IN
 * \returns the newly created widget
 */
 
static GtkWidget *CreateFileformatSelector(struct wFilSel_t *dialogBox, 
			int patterns, 
			GtkFileFilter **filters)
{
	GtkWidget *hbox = gtk_hbox_new(FALSE, 12);
	GtkWidget *text = gtk_label_new(_("Save format:"));
	GtkWidget *combo = gtk_combo_box_text_new ();

	g_signal_connect(G_OBJECT(combo), 
				 "changed",
				 (GCallback)FileFormatChanged,
				 dialogBox );


	gtk_box_pack_start (GTK_BOX(hbox),
				text,
				FALSE,
				FALSE,
				0);
	gtk_box_pack_end (GTK_BOX(hbox),
				combo,
				TRUE,
				TRUE,
				0);
	for(int i=0; i < patterns; i++ ) {
		const char *nameOfFilter = gtk_file_filter_get_name( filters[ i ] );
		gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(combo), nameOfFilter );
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX(combo), 0);
	
	gtk_widget_show_all(hbox);
	
	return(hbox);            
}

/**
 * Create a new file selector. Only the internal data structures are
 * set up, no dialog is created. 
 *
 * \param w IN parent window
 * \param mode IN ?
 * \param opt IN ?
 * \param title IN dialog title
 * \param pattList IN list of selection patterns
 * \param action IN callback 
 * \param data IN ?
 * \return    the newly created file selector structure
 */
 
struct wFilSel_t * wFilSelCreate(
	wWin_p w,
	wFilSelMode_e mode,
	int opt,
	const char * title,
	const char * pattList,
	wFilSelCallBack_p action,
	void * data )
{
	struct wFilSel_t	*fs;

	fs = (struct wFilSel_t*)malloc(sizeof *fs);
	if (!fs)
		return NULL;

	fs->parent = w;
	fs->window = 0;
	fs->mode = mode;
	fs->opt = opt;
	fs->title = strdup( title );
	fs->action = action;
	fs->data = data;
	fs->pattCount = 0;
	fs->loadPatternsAdded = FALSE;

	if (pattList) {
		char * cps = strdup(pattList);
		char *cp, *cp2;
		int count = 0;
		char *patternState, *patternState2;

		//create filters for the passed filter list
		// names and patterns are separated by |
		// filter elements are also separated by |
		cp = cps;
		while (cp && cp[0]) {
			if (cp[0] == '|') {
				count++;
				if (count && count%2==0) {
					cp[0] = ':';             //Replace every second "|" with ":"
				}
			}
			cp++;
		}
		count = 0;
		cp = cps;							//Restart
		if (opt&FS_PICTURES) {				//Put first
			fs->filter[ count ] = gtk_file_filter_new ();
			g_object_ref_sink( G_OBJECT(fs->filter[ count ] ));
			gtk_file_filter_set_name( fs->filter[ count ], _("Image files") );
			gtk_file_filter_add_pixbuf_formats( fs->filter[ count ]);
			fs->pattCount = ++count;
		}
		cp = strtok_r( cp, ":", &patternState );          // Break up by colons
		while ( cp  && count < (MAX_ALLOWEDFILTERS - 1)) {
			cp2 = strtok_r( cp, "|", &patternState2 );
			if (cp2) {
				fs->filter[ count ] = gtk_file_filter_new ();
				gtk_file_filter_set_name ( fs->filter[ count ], cp2 );

				cp2 = strtok_r( NULL, "|", &patternState2 );
				// find multiple patterns separated by ";"
				if (cp2) {
					char * cp1s = strdup(cp2);
					char *cp1;
					char *filterState;

					cp1 = cp1s;
					cp1 = strtok_r(cp1, ";", &filterState );
					while (cp1) {
						gtk_file_filter_add_pattern (fs->filter[ count ], cp1 );
						cp1 = strtok_r(NULL, ";", &filterState );
					}
					if (cp1s)
						free(cp1s);
				}
				// the first pattern is considered to match the default extension
				if( count == 0 && !(opt&FS_PICTURES)) {
					fs->defaultExtension = strdup( cp2 );
					int i = 0;
					for (i=0; i<strlen(cp2) && cp2[i] != ' ' && cp2[i] != ';';i++) ;
					if (i<strlen(cp2)) fs->defaultExtension[i] = '\0';
				}
				fs->pattCount = ++count;
			}
			cp = strtok_r( NULL, ":", &patternState );
		}
		if (cps) 
			free(cps);


	} else {
		fs->filter[ 0 ] = NULL;
		fs->pattCount = 0;
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
	char name[1024];
	char *host;
	char *file;
	int i;
	GError *err = NULL;

	if (fs->window == NULL) {
		fs->window = gtk_file_chooser_dialog_new( fs->title, 
										   GTK_WINDOW( fs->parent->gtkwin ),
										   (fs->mode == FS_LOAD ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE ),
										   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										   (fs->mode == FS_LOAD ? GTK_STOCK_OPEN : GTK_STOCK_SAVE ), GTK_RESPONSE_ACCEPT,
										   NULL );
		if (fs->window==0) abort();
		if ( fs->mode == FS_SAVE ) {
			// get confirmation before overwritting an existing file									
			gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(fs->window), TRUE );
		}
		
		/** \todo for loading a shortcut folder could be added linking to the example directory */

	}
	strcpy( name, dirName );

	gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(fs->window), name ); 
	if( fs->mode == FS_SAVE || fs->mode == FS_UPDATE ) {
		gtk_file_chooser_set_extra_widget( GTK_FILE_CHOOSER(fs->window), 
				CreateFileformatSelector(fs, fs->pattCount, fs->filter ));
	}	
    // Add a current folder and a shortcut to it for Load/import dialogs
    if( fs->mode == FS_LOAD ) {
        gtk_file_chooser_add_shortcut_folder( GTK_FILE_CHOOSER(fs->window), name, NULL );
		// allow selecting multiple files
		if( fs->opt & FS_MULTIPLEFILES ) {
			gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER(fs->window), TRUE);
		}	
		// add the file filters to the dialog box
		if( fs->pattCount && !fs->loadPatternsAdded) {

			for( i = 0; i < fs->pattCount; i++ ) {
				gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( fs->window ), fs->filter[ i ] ); 
			}
			fs->loadPatternsAdded = TRUE;
		}												
    }
    
    int resp = gtk_dialog_run( GTK_DIALOG( fs->window ));

	if( resp == GTK_RESPONSE_ACCEPT || resp == GTK_RESPONSE_APPLY) {
		char **fileNames;	
		GSList *fileNameList;
		
		fileNameList = gtk_file_chooser_get_uris( GTK_FILE_CHOOSER(fs->window) );
		fileNames = calloc( sizeof(char *), g_slist_length (fileNameList) ); 
			
		for (i=0; i < g_slist_length (fileNameList); i++ ) {
			char *namePart;

			file = g_filename_from_uri( g_slist_nth_data( fileNameList, i ), &host, &err );
			
			// check for presence of file extension
			// jump behind the last directory delimiter
			namePart = strrchr( file, '/' ) + 1;
			// is there a dot in the last part, yes->extension present
			if( !strchr( namePart, '.' ) ){
				
				// else try to find the current filter and parse its name
				GtkFileFilter *currentFilter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER(fs->window) );
				if (currentFilter) {
					const char *nameOfFilter = gtk_file_filter_get_name( currentFilter );
					char *pattern = strdup( nameOfFilter );
					char *extension = fs->defaultExtension;
					char *startDelimiter = strstr( pattern, "(*." );

					if(startDelimiter) {
						char *endDelimiter = strpbrk(startDelimiter + 3, ",;) ");
						if( endDelimiter ) {
							*endDelimiter = '\0';
							extension = startDelimiter + 2;
						}
					}
					file = g_realloc( file, strlen(file)+strlen(extension));
					strcat( file, extension );
					free( pattern );
				}
			}	
			fileNames[ i ] = file;
			g_free( g_slist_nth_data ( fileNameList, i));
		}
		
		gtk_widget_hide( GTK_WIDGET( fs->window ));
		if (fs->action) {
			fs->action( g_slist_length(fileNameList), fileNames, fs->data );
		}
		
		for(i=0; i < g_slist_length(fileNameList); i++) {
			g_free( fileNames[ i ]);
		}
		free( fileNames );
		g_slist_free (fileNameList);	
	} else {
		gtk_widget_hide( GTK_WIDGET( fs->window ));
	}
	
	return 1;
}
