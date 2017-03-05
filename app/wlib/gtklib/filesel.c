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
#include "gtkint.h"
#include "i18n.h"

#define MAX_ALLOWEDFILTERS 10

struct wFilSel_t {
		GtkWidget * window;
		wFilSelCallBack_p action;
		void * data;
		int pattCount;
		GtkFileFilter *filter[ MAX_ALLOWEDFILTERS ];
		wFilSelMode_e mode;
		int opt;
		const char * title;
		wWin_p parent;
		char *defaultExtension;
		};


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

	if (pattList) {
		char * cp = strdup(pattList);
		int count = 0;

		//create filters for the passed filter list
		// names and patterns are separated by |
		cp = strtok( cp, "|" );		
		while ( cp  && count < (MAX_ALLOWEDFILTERS - 1)) {
			fs->filter[ count ] = gtk_file_filter_new ();
			gtk_file_filter_set_name ( fs->filter[ count ], cp );
			cp = strtok( NULL, "|" );
			gtk_file_filter_add_pattern (fs->filter[ count ], cp );
			// the first pattern is considered to match the default extension
			if( count == 0 ) {
				fs->defaultExtension = strdup( cp );
			}	
			cp = strtok( NULL, "|" );
			count++;
		}
		// finally add the all files pattern
		fs->filter[ count ] = gtk_file_filter_new ();
		gtk_file_filter_set_name( fs->filter[ count ], _("All files") );
		gtk_file_filter_add_pattern( fs->filter[ count ], "*" );
		fs->pattCount = count++;
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
		// get confirmation before overwritting an existing file									
		gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(fs->window), TRUE );
		
		// allow selecting multiple files
		if( fs->opt & FS_MULTIPLEFILES ) {
			gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER(fs->window), TRUE);
		}	
		// add the file filters to the dialog box
		if( fs->pattCount ) {
			for( i = 0; i <= fs->pattCount; i++ ) {
				gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( fs->window ), fs->filter[ i ] ); 
			}
		}												
		/** \todo for loading a shortcut folder could be added linking to the example directory */

	}
	strcpy( name, dirName );

	if( fs->mode == FS_SAVE )
		gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(fs->window), name ); 
    // Add a current folder and a shortcut to it for Load/import dialogs
    if( fs->mode == FS_LOAD ) {
        gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(fs->window), name );
        gtk_file_chooser_add_shortcut_folder( GTK_FILE_CHOOSER(fs->window), name, NULL );
    }
    
	if( gtk_dialog_run( GTK_DIALOG( fs->window )) == GTK_RESPONSE_ACCEPT ) {
		char **fileNames;	
		GSList *fileNameList;
		
		fileNameList = gtk_file_chooser_get_uris( GTK_FILE_CHOOSER(fs->window) );
		fileNames = calloc( sizeof(char *), g_slist_length (fileNameList) ); 
			
		for (i=0; i < g_slist_length (fileNameList); i++ ) {
			char *namePart;

			file = g_filename_from_uri( g_slist_nth_data( fileNameList, i ), &host, &err );
			
			// check for presence of file extension
			// jump behind tha last directory delimiter
			namePart = strrchr( file, '/' ) + 1;
			// is there a dot in the last part, yes->extension present
			if( !strchr( namePart, '.' ) ){
				// make room for the extension
				file = g_realloc( file, strlen(file)+strlen(fs->defaultExtension));
				strcat( file, fs->defaultExtension + 1 );
			}	
			fileNames[ i ] = file;
			g_free( g_slist_nth_data ( fileNameList, i));
		}
		
		if (fs->data)
			strcpy( fs->data, fileNames[ 0 ] );
		
		if (fs->action) {
			fs->action( g_slist_length(fileNameList), fileNames, fs->data );
		}
		
		for(i=0; i < g_slist_length(fileNameList); i++) {
			g_free( fileNames[ i ]);
		}
		free( fileNames );
		g_slist_free (fileNameList);	
	}
	gtk_widget_hide( GTK_WIDGET( fs->window ));
	
	return 1;
}
