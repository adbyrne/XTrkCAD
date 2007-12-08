/** \file gtkhelp.c
 * Balloon help ( tooltips) and main help functions.
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkhelp.c,v 1.5 2007-12-08 02:06:11 tshead Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis and (C) 2007 Martin Fischer
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
#include <fcntl.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgtkhtml/gtkhtml.h>

#include "gtkint.h"

/* globals and defines related to the HTML help window */

#define  HTMLERRORTEXT "<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=US-ASCII\">" \
								"<title>Help Error</title><body><h1>Error - help information can not be found.</h1><p>" \
								"The help information you requested cannot be found on this system.<br>Usually this " \
								"is an installation problem, Make sure that XTrkCad and the included HTML files are " \
								"installed properly and can be found via the XTRKCADLIB environment variable. Also make " \
								"sure that the user has sufficient access rights to read these files.</p></body></html>" 


#define SLIDERPOSDEFAULT 180		/**< default value for slider position */

#define HTMLHELPSECTION    "gtklib html help" /**< section name for html help window preferences */
#define SLIDERPREFNAME	   "sliderpos"    	 /**< name for the slider position preference */
#define WINDOWPOSPREFNAME  "position"  		 /**< name for the window position preference */
#define WINDOWSIZEPREFNAME "size"   			 /**< name for the window size preference */

#define BACKBUTTON "back"
#define FORWARDBUTTON "forward"
#define TOCDOC "tocDoc"
#define CONTENTSDOC "contentsDoc"
#define TOCVIEW "viewLeft"
#define CONTENTSVIEW "viewRight"
#define PANED "hpane"

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



/*
 ******************************************************************************
 *
 * Help
 *
 ******************************************************************************
 */

wBool_t listHelpStrings = FALSE;
wBool_t listMissingHelpStrings = FALSE;
static char HelpDataKey[] = "HelpDataKey";

static GtkWidget * balloonF;
static GtkWidget * balloonPI;
static wBalloonHelp_t * balloonHelpStrings;
static int enableBalloonHelp = 1;
static const char * balloonMsg;
static wControl_p balloonB;
static wPos_t balloonDx, balloonDy;
static wBool_t balloonVisible = FALSE;
static wControl_p balloonHelpB;
#ifdef GTK
static const char * balloonHelpString;
static long balloonHelpTimeOut = 500;
static wBalloonHelp_t * currBalloonHelp;
#endif
static GtkTooltips * tooltips;


void wSetBalloonHelp( wBalloonHelp_t * bh )
{
	balloonHelpStrings = bh;
	if (!tooltips)
		tooltips = gtk_tooltips_new();
}

void wEnableBalloonHelp( int enable )
{
	enableBalloonHelp = enable;
	if (tooltips) {
		if (enable)
			gtk_tooltips_enable( tooltips );
		else
			gtk_tooltips_disable( tooltips );
	}
}


void wControlSetBalloon( wControl_p b, wPos_t dx, wPos_t dy, const char * msg )
{
	wPos_t x, y;
	wPos_t w, h;
	wPos_t xx, yy;
	const char * msgConverted;
	if (balloonVisible && balloonB == b &&
		balloonDx == dx && balloonDy == dy && balloonMsg == msg )
		return;

	if ( msg == NULL ) {
		if ( balloonF != NULL ) {
			gtk_widget_hide( balloonF );
			balloonVisible = FALSE;
		}
		balloonMsg = "";
		return;
	}
	msgConverted = gtkConvertInput(msg);
	if ( balloonF == NULL ) {
		balloonF = gtk_window_new( GTK_WINDOW_POPUP );
		gtk_window_set_policy( GTK_WINDOW (balloonF), FALSE, FALSE, TRUE );
		gtk_widget_realize( balloonF );
		balloonPI = gtk_label_new(msgConverted);
		gtk_container_add( GTK_CONTAINER(balloonF), balloonPI );
		gtk_container_border_width( GTK_CONTAINER(balloonF), 1 );
		gtk_widget_show( balloonPI );
	} else {
		gtk_label_set( GTK_LABEL(balloonPI), msgConverted );
	}
	balloonDx = dx;
	balloonDy = dy;
	balloonB = b;
	balloonMsg = msg;
	gtk_widget_hide( balloonF );
#ifndef GTK1
	w = gdk_string_width( gtk_style_get_font(gtk_widget_get_style(balloonPI)), msg );
#else
	w = gdk_string_width( balloonPI->style->font, msgConverted );
#endif
	h = 16;
	gdk_window_get_position( GTK_WIDGET(b->parent->gtkwin)->window, &x, &y );
	gdk_window_get_origin( GTK_WIDGET(b->parent->gtkwin)->window, &x, &y );
	x += b->realX + dx;
	y += b->realY + b->h - dy;
	xx = gdk_screen_width();
	yy = gdk_screen_height();
	if ( x < 0 ) {
		x = 0;
	} else if ( x+w > xx ) {
		x = xx - w;
	}
	if ( y < 0 ) {
		y = 0;
	} else if ( y+h > yy ) {
		y = yy - h ;
	}
	gtk_widget_set_usize( balloonPI, w, h );
	gtk_widget_set_usize( balloonF, w+2, h+2 );
	gtk_widget_show( balloonF );
	gtk_widget_set_uposition( balloonF, x, y );
	/*gtk_widget_popup( balloonF, x, y );*/
	gdk_draw_rectangle( balloonF->window, balloonF->style->fg_gc[GTK_STATE_NORMAL], FALSE, 0, 0, w+1, h+1 );
	gtk_widget_queue_resize( GTK_WIDGET(balloonF) );
	/*gtk_widget_set_uposition( balloonF, x, y );*/
	balloonVisible = TRUE;
}


void wControlSetBalloonText(
		wControl_p b,
		const char * label )
{
	const char * helpStr;
	if ( b->widget == NULL) abort();
	helpStr = (char*)gtk_object_get_data( GTK_OBJECT(b->widget), HelpDataKey );
	if ( helpStr == NULL ) helpStr = "NoHelp";
	if (tooltips)
		gtk_tooltips_set_tip( tooltips, b->widget, label, helpStr );
}


EXPORT void gtkHelpHideBalloon( void )
{
	if (balloonF != NULL && balloonVisible) {
		gtk_widget_hide( balloonF );
		balloonVisible = FALSE;
	}
}

#ifdef XV_help
static Notify_value showBalloonHelp( Notify_client client, int which )
{
	wControlSetBalloon( balloonHelpB, 0, 0, balloonHelpString );
	return NOTIFY_DONE;
}
#endif

static wWin_p balloonHelp_w;
static wPos_t balloonHelp_x;
static wPos_t balloonHelp_y;
static void prepareBalloonHelp( wWin_p w, wPos_t x, wPos_t y )
{
#ifdef XV
	wControl_p b;
	char * hs;
	int appNameLen = strlen( wAppName ) + 1;
	if (w == NULL)
		return;
#ifdef LATER
	if (!enableBalloonHelp)
		return;
#endif
	if (!balloonHelpStrings)
		return;

	balloonHelp_w = w;
	balloonHelp_x = x;
	balloonHelp_y = y;

	for ( b=w->first; b; b=b->next ) {
		switch ( b->type ) {
		case B_BUTTON:
		case B_CANCEL:
		case B_TEXT:
		case B_INTEGER:
		case B_LIST:
		case B_DROPLIST:
		case B_COMBOLIST:
		case B_RADIO:
		case B_TOGGLE:
		case B_DRAW:
		case B_MULTITEXT:
			if (x >= b->realX && x < b->realX+b->w &&
				y >= b->realY && y < b->realY+b->h ) {
				hs = (char*)gtk_get( b->panel_item, XV_HELP_DATA );
				if ( hs ) {
					hs += appNameLen;
					for ( currBalloonHelp = balloonHelpStrings; currBalloonHelp->name && strcmp(currBalloonHelp->name,hs) != 0; currBalloonHelp++ );
					if (currBalloonHelp->name && balloonHelpB != b && currBalloonHelp->value ) {
						balloonHelpB = b;
						balloonHelpString = currBalloonHelp->value;
						if (enableBalloonHelp) {
							wControlSetBalloon( balloonHelpB, 0, 0, balloonHelpString );
							/*setTimer( balloonHelpTimeOut, showBalloonHelp );*/
						} else {
							/*resetBalloonHelp();*/
						}
					}
					return;
				}
			}
		default:
			;
		}
	}
	cancelTimer( showBalloonHelp );
	resetBalloonHelp();
#endif
}


void wBalloonHelpUpdate( void )
{
	balloonHelpB = NULL;
	balloonMsg = NULL;
	prepareBalloonHelp( balloonHelp_w, balloonHelp_x, balloonHelp_y );
}


void gtkAddHelpString(
		GtkWidget * widget,
		const char * helpStr )
{
	int rc;
	char *string;
	wBalloonHelp_t * bhp;
	rc = 0;
	if (helpStr==NULL || *helpStr==0)
		return;
	if ( balloonHelpStrings == NULL )
		return;
	for ( bhp = balloonHelpStrings; bhp->name && strcmp(bhp->name,helpStr) != 0; bhp++ );
	if (listMissingHelpStrings && !bhp->name) {
		printf( "Missing Help String: %s\n", helpStr );
		return;
	}
	string = malloc( strlen(wAppName) + 5 + strlen(helpStr) + 1 );
	sprintf( string, "%sHelp/%s", wAppName, helpStr );
	if (tooltips)
		gtk_tooltips_set_tip( tooltips, widget, bhp->value, string );
	gtk_object_set_data( GTK_OBJECT( widget ), HelpDataKey, string );
	if (listHelpStrings)
		printf( "HELPSTR - %s\n", string );
}


EXPORT const char * wControlGetHelp(
		wControl_p b )		/* Control */
/*
Returns the help topic string associated with <b>.
*/
{
	const char * helpStr;
	helpStr = (char*)gtk_object_get_data( GTK_OBJECT(b->widget), HelpDataKey );
	return helpStr;
}


EXPORT void wControlSetHelp(
	 wControl_p b,		/* Control */
	 const char * help )		/* topic string */
/*
Set the help topic string for <b> to <help>.
*/
{
	const char * helpStr;
	if (b->widget == 0) abort();
	helpStr = wControlGetHelp(b);
	if (tooltips)
		gtk_tooltips_set_tip( tooltips, b->widget, help, helpStr );
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
 * load a HTML file into the specified doc.The base directory will be prepended before
 * the file is opmned for reading. In case the file can't be opened, a static error
 * page is output to teh document. 
 * 
 * \PARAM view IN HTML view to display in
 * \PARAM doc IN document into which the file will be loaded.
 * \PARAM file IN filename to load.
 */
 
static void
LoadHtml( GtkWidget *view, HtmlDocument *doc, const char *file )
{
  	char *filename;
	char *copyOfUrl;
	char *htmlFile;
 	char *buffer;
  	int handle;
  	int len;

	/* create a working copy of the filename, as this will be modified */
	copyOfUrl = strdup( file );
	assert( copyOfUrl != NULL );  	

	/* separate an anchor from the rest */
	filename = strtok( copyOfUrl, "#" );
	
	/* make sure that the filename as a file part and is not only an anchor */
	if( file[ 0 ] != '#' ) {
	   html_view_set_document( HTML_VIEW(view), NULL );
  
		/* assemble the full filename for the HTML file */  
  		htmlFile = malloc( strlen( directory ) + strlen( filename ) + 1 );
	  	assert( htmlFile != NULL );
  
  		strcpy( htmlFile, directory );
	  	strcat( htmlFile, filename );
	
		/* open the output stream */
		html_document_open_stream (doc, "text/html");
  
	   /* open the file */
  		handle = open( htmlFile, O_RDONLY );
	  	if( handle != -1 )
  		{
			/* if successful, read the file and output it to the document */
	  		buffer = malloc( 8192 );
			assert( buffer != NULL );
		
		   while((len = read( handle, buffer, 8192 )) > 0 ) {
			   html_document_write_stream( doc, buffer, len );
			}

			/* clean up */ 	   
  			free( buffer );
			close( handle );	

	   } else {
			/* file could not be opened, display the error page instead */  
	   	html_document_write_stream( doc, HTMLERRORTEXT, strlen( HTMLERRORTEXT ));

	   }
  

   	html_view_set_document(HTML_VIEW( view ), doc); 
	
	   /* clean up */
		html_document_close_stream (doc); 	
		free( htmlFile );

		filename = strtok( NULL, "#" );
	}
		
	if( filename ) {
		/* jump to anchor */
		html_view_jump_to_anchor( HTML_VIEW(view), filename );
	}
	  
   free( copyOfUrl );
		
		
}

/**
 * handles clicks on the forward / backward buttons.
 *
 * \PARAM widget IN button pressed
 * \PARAM data IN dialog window handle
 */

static void
DoHistory( GtkWidget *widget, void *data )
{
	GtkWidget *parent = (GtkWidget *)data;
	GtkWidget *back = lookup_widget( parent, BACKBUTTON );
	GtkWidget *forw = lookup_widget( parent, FORWARDBUTTON);
	HtmlDocument *doc = (HtmlDocument *)lookup_widget( parent, CONTENTSDOC );	
	GtkWidget *view = lookup_widget( parent, CONTENTSVIEW );
	
	if( widget == back ) {
			sHtmlHistory.curShownPage = DECBUFFERINDEX( sHtmlHistory.curShownPage );
			
			/* enable forward button */
			gtk_widget_set_sensitive( forw, TRUE );
			
			/* if oldest page is shown, disable back button */
			if( sHtmlHistory.curShownPage	== sHtmlHistory.oldestPage )
				  gtk_widget_set_sensitive( widget, FALSE );				
	} else {
		if( widget == forw ) {
			sHtmlHistory.curShownPage = INCBUFFERINDEX( sHtmlHistory.curShownPage );
			
			/* enable back button */
			gtk_widget_set_sensitive( back, TRUE );
			/* if newest page is shown, disable forward button */
			if( sHtmlHistory.curShownPage	== sHtmlHistory.newestPage )
				  gtk_widget_set_sensitive( widget, FALSE );				
		} else {	
			assert(( widget == forw || widget == back ));	
		}
	}
	
	LoadHtml( view, doc, sHtmlHistory.url[ sHtmlHistory.curShownPage ]);				
}


/**
 * Put the filename into the history stack.
 *
 * \PARAM helpWin IN handle of help window
 * \PARAM filename IN relative filename to put onto the stack
 */ 

static void
AddToHistory( GtkWidget *helpWin, char *filename )
{
	/* this handles the start case when the buffer is still empty */
	if( sHtmlHistory.bInUse )
	{
		/* starting from the second page added, curShownPage is always advanced */
		sHtmlHistory.curShownPage = INCBUFFERINDEX( sHtmlHistory.curShownPage );
		gtk_widget_set_sensitive( lookup_widget( helpWin, BACKBUTTON ), TRUE );
	}					

	gtk_widget_set_sensitive( lookup_widget( helpWin, FORWARDBUTTON ), FALSE );

	
	sHtmlHistory.newestPage = sHtmlHistory.curShownPage;

	if( sHtmlHistory.url[ sHtmlHistory.newestPage ] )
		free( sHtmlHistory.url[ sHtmlHistory.newestPage ] );
	
	/* put the filaname into the buffer */	
	sHtmlHistory.url[ sHtmlHistory.newestPage ] = strdup( filename );
	
	if( sHtmlHistory.newestPage == sHtmlHistory.oldestPage && sHtmlHistory.bInUse )
			/* the buffer pointer wrapped around, so the oldest entry will be overwritten next. 
				The relevant index now points to the second oldest entry */
			sHtmlHistory.oldestPage = INCBUFFERINDEX( sHtmlHistory.oldestPage );

	sHtmlHistory.bInUse = TRUE;				
   
	return;	
}

/**
 * handles the clicked event for the Home button. Single action is to load
 * the homepage into the contents window.
 * \PARAM widget IN ignored
 * \PARAM data IN handle for the help dialog window
 *
 */				 

static void 
LoadHomepage( GtkWidget *widget, void *data )
{
	GtkWidget *parent = (GtkWidget *)data;
	HtmlDocument *doc = (HtmlDocument *)lookup_widget( parent, CONTENTSDOC );	
	GtkWidget *view = lookup_widget( parent, CONTENTSVIEW );	
	
	
	AddToHistory( (GtkWidget *)data, "index.html" );
	  
	LoadHtml( view, doc, "index.html" );	
}

/**
 * Create all the widgets for decorating and scrolling an HTML view 
 *
 * \PARAM paned IN the pane container holding the view
 * \PARAM PackFunc IN pointer to function for adding the widget to the pane
 * \PARAM htmlView IN the HTML view widget to add
 *
 * \RETURN the scroll widget
 */
 
static GtkWidget* 
CreateHtmlViewScrollable( GtkWidget *paned, void PackFunc( GtkPaned *, GtkWidget *), GtkWidget *htmlView )
{
	GtkWidget *frame;
	GtkWidget *scroll;
	
	/* a frame */
	frame = gtk_frame_new( NULL );
   PackFunc( (GtkPaned *)paned, frame );

	/* and a scroll window holding the HTML view */
  	scroll = gtk_scrolled_window_new( NULL, NULL );
  	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
   gtk_container_add( GTK_CONTAINER( scroll ), htmlView );	

	/* put the scroll window into the frame */
	gtk_container_add( GTK_CONTAINER(frame), GTK_WIDGET(scroll));

	return scroll;
}

/**
 *	Create a new stock button. The button is created, made visible, disabled and stuffed into a container. 
 * Additionally a click handler is registered.
 *
 * \PARAM name IN Stock name of the button
 * \PARAM container IN Container into which the button is stuffed
 * \PARAM clickHandler IN handler to be called on button click
 * \PARAM userData IN data to be passed to this handler
 *
 * \RETURN button handle 
 */
 
static GtkWidget *
CreateButton( char *name, GtkWidget *container, GCallback clickHandler, void *userData )
{
	GtkWidget *button;
	
	button = gtk_button_new_from_stock( name );
   gtk_widget_set_sensitive( button, FALSE );
   gtk_widget_show( button );
   gtk_container_add( GTK_CONTAINER( container ), button );
   GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
  
   g_signal_connect( button, "clicked", clickHandler, userData );

	return( button );
}

/**
 * Create a HTML document object.
 *
 * \PARAM UrlReq IN callback for requesting URLs
 * \PARAM LinkClick IN callback for 'link clicked' events
 * \PARAM userData IN additional data to be passed with events
 *
 * \RETURN doc created document object
 */

static HtmlDocument *
CreateHtmlDoc( GCallback UrlReq, GCallback LinkClick, void *userData )
{
	HtmlDocument *doc;
	
   doc = html_document_new();

	g_signal_connect (G_OBJECT( doc ), "request_url", UrlReq, userData );

	/* whenever the user selects a link here, the contents area has to be updated */
   g_signal_connect(G_OBJECT (doc), "link_clicked", LinkClick, userData);

	return doc;
}

/**
 * Event handler for clicks onto links. 
 *
 * \PARAM doc IN affected HTML document
 * \PARAM uri IN selected resource
 * \PARAM ptr IN pointer to user data ( help window handle )
 */
 
void
LinkClicked(HtmlDocument *doc, const gchar *uri, void *ptr )
{
	GtkWidget *outView = lookup_widget( ptr, CONTENTSVIEW);
	HtmlDocument *docCont = (HtmlDocument *)lookup_widget( ptr, CONTENTSDOC );
	
	AddToHistory((GtkWidget *)ptr, (char *)uri ); 

	LoadHtml( outView, docCont, uri );

   return;
}

/**
 *	Event handler for URL requests. This handler is called for all inline URLs eg. images. 
 * The files are read and the contents is dumped into the supplied stream.
 *
 * \PARAM doc IN affected HTML document
 * \PARAM uri IN requested url
 * \PARAM stream IN stream into which the data should be written
 * \PARAM data IN unused 
 */


void
UrlRequested(HtmlDocument *doc, const gchar *uri, HtmlStream *stream, gpointer data)
{
   char *filename;
   char *buffer;
   int handle;
   int len;
  
   filename = malloc( strlen( directory ) + strlen( uri ) + 1 );
   assert( filename != NULL );
  
   strcpy( filename, directory );
   strcat( filename, uri );
  
   handle = open( filename, O_RDONLY );
   if( handle )
   {
   	buffer = malloc( 8192 );
		assert( buffer != NULL );

	   while((len = read( handle, buffer, 8192 )) > 0 ) {
		   html_stream_write( stream, buffer, len );
		}	
		close( handle );	

  		free( buffer );
   } 
  
   free( filename );
	
   html_stream_close(stream); 
	
	return;
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

/**
 * Create the help windows including all contained widgets and the needed HTML documents.
 * 
 * \RETURN handle of the created window.
 */
 
GtkWidget*
CreateHelpWindow (void)
{
   GtkWidget *wHelpWindow;
   GtkWidget *vbox2;
   GtkWidget *hbuttonbox1;
   GtkWidget *back;
   GtkWidget *forw;
   GtkWidget *button3;
   GtkWidget *hbox1;
   GtkWidget *image2;
   GtkWidget *label18;
   GtkWidget *hpaned;
   GtkWidget *viewLeft;
   GtkWidget *viewRight;
	HtmlDocument *doc1;
	HtmlDocument *doc2;
	
	int width;
  	int height;
	int x, y;
	int w = 0, h = 0;
  	const char *pref;

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

   vbox2 = gtk_vbox_new (FALSE, 0);
   gtk_widget_show (vbox2);

   gtk_container_add (GTK_CONTAINER (wHelpWindow), vbox2);

   hbuttonbox1 = gtk_hbutton_box_new ();
   gtk_widget_show (hbuttonbox1);
   gtk_box_pack_start (GTK_BOX (vbox2), hbuttonbox1, FALSE, TRUE, 0);
   gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_START);
	gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox1), 6);	
   gtk_box_set_spacing (GTK_BOX (hbuttonbox1), 6);


	/* the two navigation buttons */
	back = CreateButton( "gtk-go-back", hbuttonbox1, (GCallback)DoHistory, wHelpWindow );

	forw = CreateButton( "gtk-go-forward", hbuttonbox1, (GCallback)DoHistory, wHelpWindow ); 	

	/* as we want to set the text shown, the GTK_STOCK_HOME can't be used here */
   button3 = gtk_button_new ();
   gtk_widget_show (button3);
   gtk_container_add (GTK_CONTAINER (hbuttonbox1), button3);
   GTK_WIDGET_SET_FLAGS (button3, GTK_CAN_DEFAULT);

   hbox1 = gtk_hbox_new (FALSE, 0);
   gtk_widget_show (hbox1);
   gtk_container_add (GTK_CONTAINER (button3), hbox1);

   image2 = gtk_image_new_from_stock ("gtk-home", GTK_ICON_SIZE_BUTTON);
   gtk_widget_show (image2);
   gtk_box_pack_start (GTK_BOX (hbox1), image2, TRUE, FALSE, 0);
   gtk_misc_set_alignment (GTK_MISC (image2), 0, 0);

   label18 = gtk_label_new ("Home");
   gtk_widget_show (label18);
   gtk_box_pack_end (GTK_BOX (hbox1), label18, TRUE, TRUE, 0);
   gtk_misc_set_alignment (GTK_MISC (label18), 0, 0.5);

 	g_signal_connect( GTK_WIDGET( button3 ), "clicked", (GCallback)LoadHomepage, wHelpWindow );

	/*
	 * create the browser windows
	 */

	/* the horizontal slider */
	hpaned = CreateHPaned( GTK_BOX (vbox2), "property" );
  
   /* the left side of the slider */
   viewLeft = html_view_new ();
   html_view_set_magnification (HTML_VIEW (viewLeft), 1 );

	CreateHtmlViewScrollable( hpaned, gtk_paned_add1, viewLeft );

	doc1 = CreateHtmlDoc( (GCallback)UrlRequested, (GCallback)LinkClicked, wHelpWindow );
			
	/* the right side of the slider */
   viewRight = html_view_new ();
   html_view_set_magnification (HTML_VIEW (viewRight), 1 );

	CreateHtmlViewScrollable( hpaned, gtk_paned_add2, viewRight );

	doc2 = CreateHtmlDoc( (GCallback)UrlRequested, (GCallback)LinkClicked, wHelpWindow );
 
   /* Store pointers to all widgets, for use by lookup_widget(). */
   GLADE_HOOKUP_OBJECT_NO_REF (wHelpWindow, wHelpWindow, "wHelpWindow");
   GLADE_HOOKUP_OBJECT (wHelpWindow, back, BACKBUTTON);
   GLADE_HOOKUP_OBJECT (wHelpWindow, forw, FORWARDBUTTON);
   GLADE_HOOKUP_OBJECT (wHelpWindow, button3, "button3");
   GLADE_HOOKUP_OBJECT (wHelpWindow, hpaned, PANED );
   GLADE_HOOKUP_OBJECT (wHelpWindow, viewLeft, TOCVIEW );
   GLADE_HOOKUP_OBJECT (wHelpWindow, viewRight, CONTENTSVIEW );
   GLADE_HOOKUP_OBJECT_NO_REF (wHelpWindow, (GtkWidget *)doc1, TOCDOC );
   GLADE_HOOKUP_OBJECT_NO_REF (wHelpWindow, (GtkWidget *)doc2, CONTENTSDOC );
	

   return wHelpWindow;
}

/**
 * Invoke the help system to display help for <topic>.
 *
 * \param topic IN topic string
 */

EXPORT void wHelp( const char * topic )		
{
   HtmlDocument *docToc;  
   HtmlDocument *docContents;  
   GtkWidget *view;
   GtkWidget *viewToc;
	char *htmlFile;


	if( !wHelpWindow )
	{
		wHelpWindow = CreateHelpWindow();
		directory = malloc( BUFSIZ );
		assert( directory != NULL );
			
		sprintf( directory, "%s/html/", wGetAppLibDir());
		
		/* initialize the left pane of the window */
		viewToc = lookup_widget( wHelpWindow, TOCVIEW);
		docToc = (HtmlDocument *)lookup_widget( wHelpWindow, TOCDOC ); 		

	   /* load and show the table of contents */
		LoadHtml( viewToc, docToc, "contents.html" );
	}

	/* display the requested page in the main pane */
	view = lookup_widget( wHelpWindow, CONTENTSVIEW);
	docContents = (HtmlDocument *)lookup_widget( wHelpWindow, CONTENTSDOC );
	
	/* need space for the 'html' extension plus dot plus \0 */
	htmlFile = malloc( strlen( topic ) + 6 );
	assert( htmlFile != NULL );
	
	sprintf( htmlFile, "%s.html", topic );
	
	AddToHistory( wHelpWindow, htmlFile );
   LoadHtml( view, docContents, htmlFile );

   gtk_widget_show_all (wHelpWindow);
	gtk_window_present( wHelpWindow );
}

/**
 * Handle the commands issued from the Help drop-down. Currently, we only have a table 
 * of contents, but search etc. might be added in the future.
 *
 * \PARAM data IN command value 
 *
 */
 
static void 
DoHelpMenu( void *data )
{
	int func = (int)data;
	
	switch( func )
	{
		case 1:
			wHelp( "index" );
			break;
		default:
			break;
	}
	
	return;		
}

/**
 * Add the entries for Help to the drop-down.
 * 
 * \PARAM m IN handle of drop-down
 *
 */

void wMenuAddHelp( wMenu_p m )
{
	wMenuPushCreate( m, NULL, "&Contents", 0, DoHelpMenu, (void*)1 );
}
