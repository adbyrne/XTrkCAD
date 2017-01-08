/** \file ixhelp.c
 * HTML based help using webkitgtk
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2015 Martin Fischer
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
#include <dirent.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <stdint.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <webkit/webkit.h>

#include "gtkint.h"
#include "i18n.h"

void load_into_view (char *file, int requested_view);	// Prototype to please clang.

/* globals and defines related to the HTML help window */

#define  HTMLERRORTEXT "<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=US-ASCII\">" \
								"<title>Help Error</title><body><h1>Error - help information can not be found.</h1><p>" \
								"The help information you requested cannot be found on this system.<br><pre>%s: %s</pre><p>" \
								"Usually this is an installation problem, Make sure that XTrackCAD and the included " \
								"HTML files are installed properly and can be found via the XTRKCADLIB environment " \
								"variable. Also make sure that the user has sufficient access rights to read these" \
 								"files.</p></body></html>" 


#define SLIDERPOSDEFAULT 180		/**< default value for slider position */

#define HTMLHELPSECTION    "gtklib html help" /**< section name for html help window preferences */
#define SLIDERPREFNAME	   "sliderpos"    	 /**< name for the slider position preference */
#define WINDOWPOSPREFNAME  "position"  		 /**< name for the window position preference */
#define WINDOWSIZEPREFNAME "size"   			 /**< name for the window size preference */

#define BACKBUTTON "back"
#define FORWARDBUTTON "forward"
#define HOMEBUTTON "home"
#define CONTENTBUTTON "contents"
#define TOCDOC "tocDoc"
#define CONTENTSDOC "contentsDoc"
#define TOCVIEW "viewLeft"
#define CONTENTSVIEW "viewRight"
#define PANED "hpane"

enum pane_views { MAIN_VIEW, CONTENTS_VIEW };

#define MAXHISTORYSIZE 20

/** \struct htmlHistory 
 * for storing information about the browse history 
 */
struct htmlHistory {					
	int curShownPage;					/**< index of page that is shown currently */
	int newestPage;					/**< index of newest page loaded */
	int oldestPage;					/**< index of earliest page loaded */
	int bInUse;							/**< does buffer have at least one entry */
	char *url[ MAXHISTORYSIZE ]; 	/**< array of pages in history */
};	

static struct htmlHistory sHtmlHistory;

#define INCBUFFERINDEX( x ) (((x) + 1 ) % MAXHISTORYSIZE )
#define DECBUFFERINDEX( x ) ((x) == 0 ? MAXHISTORYSIZE - 1 : (x)-1)

static char *directory;				/**< base directory for HTML files */

static GtkWidget *wHelpWindow;	/**< handle for the help window */
static GtkWidget *main_view; /** handle for the help main data pane */
static GtkWidget *contents_view; /** handle for the help contents pane */

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

static GtkWidget*
lookup_widget(GtkWidget *widget, const gchar *widget_name)
{
  GtkWidget *parent, *found_widget;

  for (;;)
    {
      if (GTK_IS_MENU (widget))
        parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
        parent = widget->parent;
      if (!parent)
        parent = (GtkWidget*) g_object_get_data (G_OBJECT (widget), "GladeParentKey");
      if (parent == NULL)
        break;
      widget = parent;
    }

  found_widget = (GtkWidget*) g_object_get_data (G_OBJECT (widget),
                                                 widget_name);
  if (!found_widget)
    g_warning ("Widget not found: %s", widget_name);
  return found_widget;
}


/**
 * create a new horizontal pane and place it into container. 
 * The separator position is read from the resource configuration and set accordingly. 
 * Also a callback is specified that will be executed when the slider has been moved.
 *
 * \PARAM container IN the container into which the pane will be stuffed.
 * \PARAM property IN the name of the property for the slider position
 *
 * \return the HPaned handle
 */

GtkWidget *
CreateHPaned( GtkBox *container, char *property )
{
	GtkWidget *hpaned;
	long posSlider;
		
	/* the horizontal slider */
	hpaned = gtk_hpaned_new ();
	gtk_container_set_border_width (GTK_CONTAINER (hpaned), 6);	
	
   wPrefGetInteger( HTMLHELPSECTION, SLIDERPREFNAME, &posSlider, SLIDERPOSDEFAULT );
 	gtk_paned_set_position (GTK_PANED (hpaned), (int)posSlider); 
	
   /* pack the horizontal slider into the main window */
   gtk_box_pack_start( container, hpaned, TRUE, TRUE, 0 );
	gtk_widget_show( hpaned );
	
	return( hpaned );
}  

/**
 * Handler for the delete-event issued on the help window.We are saving window
 * information (eg. position) and are hiding the window instead of closing it.
 *
 * \PARAM win IN the window to be destroyed
 * \PARAM event IN unused
 * \PARAM ptr IN unused 
 *
 * \RETURN FALSE
 */
 
static gboolean
DestroyHelpWindow( GtkWidget *win, GdkEvent *event, void *ptr )
{
	int i;
	GtkWidget *widget;
	char tmp[ 20 ];
	
	gint x, y;
	
	/* get the slider position and save it */
	widget = lookup_widget( win, PANED );
	i = gtk_paned_get_position( GTK_PANED( widget ));
	wPrefSetInteger( HTMLHELPSECTION, SLIDERPREFNAME, i );
	
	/* get the window position */
	gtk_window_get_position( (GtkWindow *)win, &x, &y ); 
	sprintf( tmp, "%d %d", x, y );
	wPrefSetString( HTMLHELPSECTION, WINDOWPOSPREFNAME, tmp );
	
	/* get the window size */	
	gtk_window_get_size( (GtkWindow *)win , &x, &y );
	sprintf( tmp, "%d %d", x, y );
	wPrefSetString( HTMLHELPSECTION, WINDOWSIZEPREFNAME, tmp );
	
	gtk_widget_hide( win );
	return TRUE;
}

void back_button_clicked(GtkWidget *widget, gpointer data) {
        webkit_web_view_go_back(WEBKIT_WEB_VIEW(data));
}

void forward_button_clicked(GtkWidget *widget, gpointer data) {
        webkit_web_view_go_forward(WEBKIT_WEB_VIEW(data));
}

void home_button_clicked(GtkWidget *widget, gpointer data) {
        load_into_view("index.html", MAIN_VIEW);
}

/* Toggles the contents pane */
void contents_button_clicked(GtkWidget *widget, gpointer data) {
        if (gtk_paned_get_position(GTK_PANED(data)) < 50) {
                gtk_paned_set_position(GTK_PANED(data), 370);
        }
        else {
                gtk_paned_set_position(GTK_PANED(data), 0);
        }
}

gboolean contents_click_handler(
                WebKitWebView *web_view, 
                WebKitWebFrame *frame, 
                WebKitNetworkRequest *request, 
                WebKitWebNavigationAction *navigation_action, 
                WebKitWebPolicyDecision *policy_decision, 
                gpointer data) {

        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(data), webkit_network_request_get_uri(request));

        return TRUE;
}

/**
 * Initialize the buttons for the help window
 */
void initialize_buttons (GtkWidget *main_vbox, GtkWidget *content_hpane) {
        GtkWidget *buttons_hbuttonbox;
        GtkWidget *back_button;
        GtkWidget *forward_button;
        GtkWidget *home_button;
        GtkWidget *contents_button;

        // define and attach signals to buttons
        back_button = gtk_button_new_with_label(_("Back"));
        g_signal_connect(back_button, "clicked", G_CALLBACK(back_button_clicked), G_OBJECT(main_view));

        forward_button = gtk_button_new_with_label(_("Forward"));
        g_signal_connect(forward_button, "clicked", G_CALLBACK(forward_button_clicked), G_OBJECT(main_view));

        home_button = gtk_button_new_with_label(_("Home"));
        g_signal_connect(home_button, "clicked", G_CALLBACK(home_button_clicked), G_OBJECT(main_view));

        contents_button = gtk_button_new_with_label(_("Contents"));
        g_signal_connect(contents_button, "clicked", G_CALLBACK(contents_button_clicked), G_OBJECT(content_hpane));

        // button layout
        buttons_hbuttonbox = gtk_hbutton_box_new();
        gtk_container_add(GTK_CONTAINER(buttons_hbuttonbox), back_button);
        gtk_container_add(GTK_CONTAINER(buttons_hbuttonbox), forward_button);
        gtk_container_add(GTK_CONTAINER(buttons_hbuttonbox), home_button);
        gtk_container_add(GTK_CONTAINER(buttons_hbuttonbox), contents_button);
        gtk_box_pack_start(GTK_BOX(main_vbox), buttons_hbuttonbox, FALSE, TRUE, 0);
        gtk_box_set_spacing(GTK_BOX(buttons_hbuttonbox), 6);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(buttons_hbuttonbox), GTK_BUTTONBOX_START);

	/* Store pointers to all widgets, for use by lookup_widget().  */
	GLADE_HOOKUP_OBJECT (main_view, back_button, BACKBUTTON);
	GLADE_HOOKUP_OBJECT (main_view, forward_button, FORWARDBUTTON);
	GLADE_HOOKUP_OBJECT (main_view, home_button, HOMEBUTTON);
	GLADE_HOOKUP_OBJECT (main_view, contents_button, CONTENTBUTTON);
}

/**
 * Create the help windows including all contained widgets and the needed HTML documents.
 * 
 * \RETURN handle of the created window.
 */
 
GtkWidget*
CreateHelpWindow (void)
{
        GtkWidget *main_vbox;
        GtkWidget *main_view_scroller;
        GtkWidget *contents_view_scroller;
        GtkWidget *content_hpane;
	
	int width;
  	int height;
	int x, y;
	int w = 0, h = 0;
  	const char *pref;
	char title[100]; 

 	wHelpWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	width = gdk_screen_get_width( gtk_window_get_screen( (GtkWindow *)wHelpWindow ));
	height = gdk_screen_get_height( gtk_window_get_screen( (GtkWindow *)wHelpWindow ));

	pref = wPrefGetString( HTMLHELPSECTION, WINDOWSIZEPREFNAME );
	if( pref ) {
		sscanf( pref, "%d %d", &w, &h );
		if( w > width )
			w = width;
		if( h > height )
			h = height;				
	}
	else {
		w = ( width * 2 )/ 5;
		h = height - 100;
	}

	pref = wPrefGetString( HTMLHELPSECTION, WINDOWPOSPREFNAME );
	if( pref ) {
		sscanf( pref, "%d %d", &x, &y );
		if( y > height - h )
			y = height - h;
			
		if( x > width - w )
			x = width - w;		
	}
	else {
		x = ( width * 3 ) / 5 - 10;
		y = 70;
	}
	
	gtk_window_resize( (GtkWindow *)wHelpWindow, w, h );
	gtk_window_move( (GtkWindow *)wHelpWindow, x, y );

	gtk_window_set_title (GTK_WINDOW (wHelpWindow), "XTrkCad Help");

	g_signal_connect( G_OBJECT( wHelpWindow ), "delete-event", G_CALLBACK( DestroyHelpWindow ), NULL );

	main_view_scroller = gtk_scrolled_window_new(NULL, NULL);
	contents_view_scroller = gtk_scrolled_window_new(NULL, NULL);
	main_view = webkit_web_view_new();
	contents_view = webkit_web_view_new();
	// must be done here as it gets locked down later
	load_into_view ("contents.html", CONTENTS_VIEW);
	gtk_widget_set_size_request(GTK_WIDGET(wHelpWindow), x, y);

	main_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(wHelpWindow), main_vbox);

	gtk_container_add(GTK_CONTAINER(main_view_scroller), main_view);

	gtk_container_add(GTK_CONTAINER(contents_view_scroller), contents_view);

	content_hpane = gtk_hpaned_new();
	initialize_buttons(main_vbox, content_hpane);
	gtk_container_add(GTK_CONTAINER(content_hpane), contents_view_scroller);
	gtk_container_add(GTK_CONTAINER(content_hpane), main_view_scroller);
	gtk_box_pack_start(GTK_BOX(main_vbox), content_hpane, TRUE, TRUE, 0);

	gtk_paned_set_position(GTK_PANED(content_hpane), 370);

	g_signal_connect(contents_view, "navigation-policy-decision-requested", G_CALLBACK(contents_click_handler), G_OBJECT(main_view));

	/* Store pointers to all widgets, for use by lookup_widget().  */
	GLADE_HOOKUP_OBJECT_NO_REF (wHelpWindow, wHelpWindow, "wHelpWindow");
	GLADE_HOOKUP_OBJECT (wHelpWindow, content_hpane, PANED );
	GLADE_HOOKUP_OBJECT (wHelpWindow, contents_view, TOCVIEW );
	GLADE_HOOKUP_OBJECT (wHelpWindow, main_view, CONTENTSVIEW );

	return wHelpWindow;
}

void load_into_view (char *file, int requested_view) {
	GtkWidget *view;
 
	switch (requested_view) {
	       case MAIN_VIEW:
	               view = main_view;
	               break;
	       case CONTENTS_VIEW:
	               view = contents_view;
	               break;
	       default:
	               printf("*** error, could not find view");
	               break;
	}

	char fileToLoad[250] = "file://";
	strcat(fileToLoad,directory);
	strcat(fileToLoad,file);

	//debug printf("*** loading %s into pane %d.\n", fileToLoad, requested_view);
	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(view), fileToLoad);
}

/**
 * Invoke the help system to display help for <topic>.
 *
 * \param topic IN topic string
 */

EXPORT void wHelp( const char * topic )		
{
	char *htmlFile;

	if( !wHelpWindow )
	{
		directory = malloc( BUFSIZ );
		assert( directory != NULL );
			
		sprintf( directory, "%s/html/", wGetAppLibDir());
		
		wHelpWindow = CreateHelpWindow();
		/* load the default content */
		load_into_view ("index.html", MAIN_VIEW);
	}

	/* need space for the 'html' extension plus dot plus \0 */
	htmlFile = malloc( strlen( topic ) + 6 );
	assert( htmlFile != NULL );
	
	sprintf( htmlFile, "%s.html", topic );
	
	load_into_view (htmlFile, MAIN_VIEW);
	gtk_widget_show_all(wHelpWindow);
	gtk_window_present(GTK_WINDOW(wHelpWindow));
}
