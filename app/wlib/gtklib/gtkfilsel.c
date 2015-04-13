/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkfilsel.c,v 1.5 2009-05-15 18:54:20 m_fischer Exp $
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

#ifdef LATER
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <unistd.h>
#include <string.h>

#include "gtkint.h"
#include "i18n.h"

/* #define FILSELWAIT */



struct wFilSel_t {
		GtkWidget * window;
		wFilSelCallBack_p action;
		void * data;
		const char * pattList;
		int pattCount;
		char **patts;
		wFilSelMode_e mode;
 	int opt;
		const char * title;
		};

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
	const char * patts[30];

	fs = (struct wFilSel_t*)malloc(sizeof *fs);
	if (!fs)
		return NULL;

	fs->window = 0;
	fs->mode = mode;
	fs->opt = opt;
	fs->title = strdup( title );
	fs->pattList = strdup(pattList);
	fs->action = action;
	fs->data = data;

	if (pattList) {
		cp = strdup(pattList);
		count = 1;
		cp = strtok( cp, "|" );
		patts[0] = strtok( NULL, "|" );
		while ( cp && count < sizeof patts/sizeof patts[0] ) {
			cp = strtok( NULL, "|" );
			patts[count++] = strtok( NULL, "|" );
		}
		while (patts[count-1] == NULL) count--;
		fs->patts = (char**)malloc( count * sizeof patts[0] );
		memcpy( (void*)fs->patts, patts, count * sizeof patts[0] );
		fs->pattCount = count;
	} else {
		fs->patts = NULL;
		fs->pattCount = 0;
	}
	return fs;
}

static int file_selection_ok (
		GtkWidget        *w,
		struct wFilSel_t *fs)
{
	const char * path;
	const char * base;
	char tmp[1024+40];
	char * cp;
	GtkWidget * window;
	if (fs == 0)
		return 1;
	window = fs->window;
	if ( window == NULL )
		return 1;
	path = gtk_file_selection_get_filename( GTK_FILE_SELECTION(window) );
	if (path == NULL)
		return 1;
	strcpy( tmp, path );
	if ((fs->mode == FS_SAVE || fs->mode == FS_UPDATE) && fs->pattCount) {
		cp = tmp+strlen(tmp);
		cp -= strlen(fs->patts[0])-1;
		if ( strcmp( cp, fs->patts[0]+1 ) != 0 ) {
			strcat( tmp, fs->patts[0]+1 );
		}
		if ( access( tmp, F_OK ) == 0 && fs->mode == FS_SAVE ) {
			cp = tmp+strlen(tmp);
			strcat(cp, _(" exists\nDo you want to overwrite it?") );
			if ( wNoticeEx( NT_WARNING, tmp, _("Yes"), _("No") ) <= 0 )
				return 0;
			*cp = '\0';
		}
	}
	if (fs->data)
		strcpy( fs->data, tmp );
	gtkDoModal( NULL, FALSE );
	fs->window = NULL;
	gtk_widget_destroy( GTK_WIDGET(window) );
	if (fs->action) {
		base = strrchr( tmp, '/' );
		if (base==0) {
			fprintf(stderr,"no / in %s\n", tmp );
			return 1;
		}
		fs->action( tmp, base+1, fs->data );
	}
	return 1;
}


static int file_selection_cancel (
		GtkWidget        *w,
		struct wFilSel_t *fs)
{
	GtkWidget * window = fs->window;
	if ( window == NULL )
		return 1;
	gtkDoModal( NULL, FALSE );
	fs->window = NULL;
	gtk_widget_destroy( GTK_WIDGET(window) );
	return 1;
}


#ifdef LATER
static gint filesel_event(
		GtkWidget * widget,
		GdkEventKey *event )
{
	printf( "filesel_event\n" );
	return 0;
}

static gint filelist_event(
		GtkWidget * widget,
		GdkEventKey *event )
{
	printf( "filelist_event\n" );
	return 0;
}
#endif


int wFilSelect( struct wFilSel_t * fs, const char * dirName )
{
	char name[1024];
	char * cp;
	if (fs->window == NULL) {
		fs->window = gtk_file_selection_new( fs->title );
		if (fs->window==0) abort();
		gtk_window_position( GTK_WINDOW(fs->window), GTK_WIN_POS_MOUSE );
		gtk_signal_connect (GTK_OBJECT (fs->window), "destroy",
						  GTK_SIGNAL_FUNC(file_selection_cancel),
						  fs );

		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (fs->window)->ok_button),
						  "clicked", GTK_SIGNAL_FUNC(file_selection_ok),
						  fs );
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (fs->window)->cancel_button),
						  "clicked", GTK_SIGNAL_FUNC(file_selection_cancel),
						  fs );
#ifdef LATER
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (fs->window)),
						  "event", GTK_SIGNAL_FUNC(filesel_event),
						  fs );
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (fs->window)->file_list),
						  "event", GTK_SIGNAL_FUNC(filelist_event),
						  fs );
#endif
	}
	strcpy( name, dirName );
	cp = name+strlen(name);
	if (cp[-1] != '/') {
		*cp++ = '/';
		*cp = 0;
	}
	gtk_file_selection_set_filename( GTK_FILE_SELECTION(fs->window), name ); 
	if ( fs->patts && fs->patts[0] )
		gtk_file_selection_complete( GTK_FILE_SELECTION(fs->window), fs->patts[0] );
	gtk_widget_show( fs->window );
	gtkDoModal( NULL, TRUE );
	return 1;
}
