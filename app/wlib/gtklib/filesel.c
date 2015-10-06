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

#include "gtkint.h"
#include "i18n.h"

struct wFilSel_t {
		GtkWidget * window;
		wFilSelCallBack_p action;
		void * data;
		int pattCount;
		GtkFileFilter *filter[ 10 ];
		wFilSelMode_e mode;
		int opt;
		const char * title;
		wWin_p parent;
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
	int count;
	char * cp;
	GtkFileFilter *filter;

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
		//create filters for the passed filter list
		cp = strdup(pattList);
		count = 0;
		// names and patterns are separated by |
		cp = strtok( cp, "|" );		
		while ( cp  && count < 9 ) {
			fs->filter[ count ] = gtk_file_filter_new ();
			gtk_file_filter_set_name ( fs->filter[ count ], cp );
			cp = strtok( NULL, "|" );
			gtk_file_filter_add_pattern (fs->filter[ count ], cp );
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
	char *fileName;
	const char *base;
	int i;
	
	char * cp;
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
		
		// add the file filters to the dialog box
		if( fs->pattCount ) {
			for( i = 0; i <= fs->pattCount; i++ ) {
				gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( fs->window ), fs->filter[ i ] ); 
			}
		}												
		/** \todo for loading a shortcut folder could be added linking to the example directory */

	}
	strcpy( name, dirName );
	cp = name+strlen(name);
	if (cp[-1] != '/') {
		*cp++ = '/';
		*cp = 0;
	}
	if( fs->mode == FS_SAVE )
		gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(fs->window), name ); 

	if( gtk_dialog_run( GTK_DIALOG( fs->window )) == GTK_RESPONSE_ACCEPT ) {
		fileName = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(fs->window) );
			if (fs->data)
		strcpy( fs->data, fileName );
		if (fs->action) {
			base = strrchr( fileName, '/' );
			if (base==0) {
				fprintf(stderr,"no / in %s\n", fileName );
				return 1;
			}
			fs->action( fileName, base+1, fs->data );
		}
	}	
	gtk_widget_hide( GTK_WIDGET( fs->window ));
	
	return 1;
}
