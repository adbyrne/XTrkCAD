/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkmisc.c,v 1.15 2009-10-03 04:49:01 dspagnol Exp $
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
#include <dirent.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gtkint.h"
#include "i18n.h"

wWin_p gtkMainW;

long debugWindow = 0;

char wAppName[256];
char wConfigName[ 256 ];

#define FOUR		(4)
#ifndef GTK1
#define MENUH		(24)
#else
#define MENUH		(24)
#endif
#define LABEL_OFFSET	(3)
		
const char * wNames[] = {
		"MAIN",
		"POPUP",
		"BUTT",
		"CANCEL",
		"POPUP",
		"TEXT",
		"INTEGER",
		"FLOAT",
		"LIST",
		"DROPLIST",
		"COMBOLIST",
		"RADIO",
		"TOGGLE",
		"DRAW",
		"MENU"
		"MULTITEXT",
		"MESSAGE",
		"LINES",
		"MENUITEM",
		"BOX"
		};

static struct timeval startTime;

static wBool_t reverseIcon =
#if defined(linux)
				FALSE;
#else
				TRUE;
#endif

char gtkAccelChar;


/*
 *****************************************************************************
 *
 * Internal Utility functions
 *
 *****************************************************************************
 */

unsigned char gtkBitrotate(
		char v )
{
	unsigned char r = 0;
	int i;
	static unsigned char bits[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	for (i=0;i<8;i++)
		if (v & bits[i])
			r |= bits[7-i];
	return r;
}

GdkPixmap* gtkMakeIcon(
		GtkWidget * widget,
		wIcon_p ip,
		GdkBitmap ** mask )
{
	GdkPixmap * pixmap;
	char ** pixmapData;
	char * oldline1;
	static char newline1[] = " \tc None s None";
	char line0[40];
	char line2[40];
	int row,col,wb;
	long rgb;
	const char * bits;
	GdkColor *transparent;
	
	transparent = &gtk_widget_get_style( gtkMainW->gtkwin )->bg[GTK_WIDGET_STATE( gtkMainW->gtkwin )];
	
	if ( ip->gtkIconType == gtkIcon_pixmap ) {
		pixmap = gdk_pixmap_create_from_xpm_d( gtkMainW->gtkwin->window, mask, transparent, (char**)ip->bits );
	}
	else {
		wb = (ip->w+7)/8;
		pixmapData = (char**)malloc( (3+ip->h) * sizeof *pixmapData );
		pixmapData[0] = line0;
		rgb = wDrawGetRGB(ip->color);
		sprintf( line0, " %d %d 2 1", ip->w, ip->h );
		sprintf( line2, "# c #%2.2lx%2.2lx%2.2lx", (rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF );
		pixmapData[1] = ". c None s None";
		pixmapData[2] = line2;
		bits = ip->bits;
		for (row = 0; row<ip->h; row++ ) {
			pixmapData[row+3] = (char*)malloc( (ip->w+1) * sizeof **pixmapData );
			for (col = 0; col<ip->w; col++ ) {
				if ( bits[ row*wb+(col>>3) ] & (1<<(col&07)) ) {
					pixmapData[row+3][col] = '#';
				} else {
					pixmapData[row+3][col] = '.';
				}
			}
			pixmapData[row+3][ip->w] = 0;
		}
		pixmap = gdk_pixmap_create_from_xpm_d( gtkMainW->gtkwin->window, mask, transparent, pixmapData );
		for (row = 0; row<ip->h; row++ ) {
			free( pixmapData[row+3] );
		}
	}
	return pixmap;
}


int gtkAddLabel( wControl_p b, const char * labelStr )
{
	GtkRequisition requisition;
	if (labelStr == NULL)
		return 0;
	b->label = gtk_label_new(gtkConvertInput(labelStr));
	gtk_widget_size_request( b->label, &requisition );
	gtk_container_add( GTK_CONTAINER(b->parent->widget), b->label );
#ifndef GTK1
	gtk_fixed_move( GTK_FIXED(b->parent->widget), b->label, b->realX-requisition.width-8, b->realY+LABEL_OFFSET );
#else
	gtk_widget_set_uposition( b->label, b->realX-requisition.width-8, b->realY+LABEL_OFFSET );
#endif
	gtk_widget_show( b->label );
	return requisition.width+8;
}


void * gtkAlloc(
		wWin_p parent,
		wType_e type,
		wPos_t origX,
		wPos_t origY,
		const char * labelStr,
		int size,
		void * data )
{
	wControl_p w = (wControl_p)malloc( size );
	char * cp;
	memset( w, 0, size );
	if (w == NULL)
		abort();
	w->type = type;
	w->parent = parent;
	w->origX = origX;
	w->origY = origY;
	gtkAccelChar = 0;
	if (labelStr) {
		cp = (char*)malloc(strlen(labelStr)+1);
		w->labelStr = cp;
		for ( ; *labelStr; labelStr++ )
			if ( *labelStr != '&' )
				*cp++ = *labelStr;
			else {
/*				*cp++ = '_';
				gtkAccelChar = labelStr[1]; */
			}	
		*cp = 0;
	}
	w->doneProc = NULL;
	w->data = data;
	return w;
}


void gtkComputePos(
		wControl_p b )
{
	wWin_p w = b->parent;

	if (b->origX >= 0) 
		b->realX = b->origX;
	else
		b->realX = w->lastX + (-b->origX) - 1;
	if (b->origY >= 0) 
		b->realY = b->origY + FOUR + ((w->option&F_MENUBAR)?MENUH:0);
	else
		b->realY = w->lastY + (-b->origY) - 1;
}


void gtkControlGetSize(
		wControl_p b )
{
	GtkRequisition requisition;
	gtk_widget_size_request( b->widget, &requisition );
	b->w = requisition.width;
	b->h = requisition.height;
}
	

void gtkAddButton(
		wControl_p b )
{
	wWin_p win = b->parent;
	wBool_t resize = FALSE;
	if (win->first == NULL) {
		win->first = b;
	} else {
		win->last->next = b;
	}
	win->last = b;
	b->next = NULL;
	b->parent = win;
	win->lastX = b->realX + b->w;
	win->lastY = b->realY + b->h;
	if (win->option&F_AUTOSIZE) {
		if (win->lastX > win->realX) {
			win->realX = win->lastX;
			if (win->w != (win->realX + win->origX)) {
				resize = TRUE;
				win->w = (win->realX + win->origX);
			}
		}
		if (win->lastY > win->realY) {
			win->realY = win->lastY;
			if (win->h != (win->realY + win->origY)) {
			resize = TRUE;
				win->h = (win->realY + win->origY);
			}
		}
		if (win->shown) {
			if ( resize ) {
#ifndef GTK1
				gtk_widget_set_size_request( win->gtkwin, win->w, win->h );
				gtk_widget_set_size_request( win->widget, win->w, win->h );
#else
				gtk_widget_set_usize( win->gtkwin, win->w, win->h );
				gtk_widget_set_usize( win->widget, win->w, win->h );
#endif
			}
		}
	}
}


void gtkSetReadonly( wControl_p b, wBool_t ro )
{
	if (ro)
		b->option |= BO_READONLY;
	else
		b->option &= ~BO_READONLY;
}


wControl_p gtkGetControlFromPos(
		wWin_p win,
		wPos_t x,
		wPos_t y )
{
	wControl_p b;
	wPos_t xx, yy;
	for (b=win->first; b != NULL; b = b->next) {
		if ( b->widget && GTK_WIDGET_VISIBLE(b->widget) ) {
			xx = b->realX;
			yy = b->realY;
			if ( xx <= x && x < xx+b->w &&
				 yy <= y && y < yy+b->h ) {
				return b;
			}
		}
	}
	return NULL;
}

/* \brief Convert label string from Windows mnemonic to GTK
 *
 * The first occurence of '&' in the passed string is changed to '_'
 *
 * \param label the string to convert
 * \return pointer to modified string, has to be free'd after usage
 *
 */
static 
char * gtkChgMnemonic( char *label )
{
	char *ptr;
	char *cp;
	
	cp = strdup( label );
	
	ptr = strchr( cp, '&' );
	if( ptr )
		*ptr = '_';
		
	return( cp );	
}


/*
 *****************************************************************************
 *
 * Exported Utility Functions 
 *
 *****************************************************************************
 */

EXPORT void wBeep(
		void )
/*
Beep!
*/
{
	gdk_display_beep(gdk_display_get_default());
}

typedef struct {
		GtkWidget * win;
		GtkWidget * label;
		GtkWidget * butt[3];
		} notice_win;
static notice_win noticeW;
static long noticeValue;

static void doNotice(
		GtkWidget * widget,
		long value )
{
	noticeValue = value;
	gtk_widget_destroy( noticeW.win );
	gtkDoModal( NULL, FALSE );
}

/**
 * Show a notification window with a yes/no reply and an icon.
 *
 * \param type IN type of message: Information, Warning, Error
 * \param msg  IN message to display
 * \param yes  IN text for accept button
 * \param no   IN text for cancel button
 * \return    True when accept was selected, false otherwise
 */

int wNoticeEx( int type, 
       		const char * msg,
       		const char * yes,
       		const char * no )
{
	
	int res;
	unsigned flag;
	char *headline;
	GtkWidget *dialog;
	GtkWindow *parent = GTK_WINDOW_TOPLEVEL;

	switch( type ) {
		case NT_INFORMATION:
			flag = GTK_MESSAGE_INFO;
			headline = _("Information");
			break;
		case NT_WARNING:
			flag = GTK_MESSAGE_WARNING;
			headline = _("Warning");
			break;
		case NT_ERROR:
			flag = GTK_MESSAGE_ERROR;
			headline = _("Error");
			break;
	}
	
	if( gtkMainW )
		parent = GTK_WINDOW( gtkMainW->gtkwin);
	
	dialog = gtk_message_dialog_new( parent,  
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 flag,
      					 ((no==NULL)?GTK_BUTTONS_OK:GTK_BUTTONS_YES_NO),
					 "%s", msg );
	gtk_window_set_title( GTK_WINDOW(dialog), headline );
	  
	res = gtk_dialog_run( GTK_DIALOG(dialog));
	gtk_widget_destroy( dialog );
      
	return res == GTK_RESPONSE_OK  || res == GTK_RESPONSE_YES; 
}


EXPORT int wNotice(
		const char * msg,		/* Message */
		const char * yes,		/* First button label */
		const char * no )		/* Second label (or 'NULL') */
/*
Popup up a notice box with one or two buttons.
When this notice box is displayed the application is paused and
will not response to other actions.

Pushing the first button returns 'TRUE'.
Pushing the second button (if present) returns 'FALSE'.
*/
{
	return wNotice3( msg, yes, no, NULL );
}

/** \brief Popup a notice box with three buttons.
 *
 * Popup up a notice box with three buttons.
 * When this notice box is displayed the application is paused and
 * will not response to other actions.
 *
 * Pushing the first button returns 1
 * Pushing the second button returns 0
 * Pushing the third button returns -1
 *
 * \param msg Text to display in message box
 * \param yes First button label
 * \param no  Second label (or 'NULL')
 * \param cancel Third button label (or 'NULL') 
 *
 * \returns 1, 0 or -1
 */
 
EXPORT int wNotice3(
		const char * msg,		/* Message */
		const char * affirmative,		/* First button label */
		const char * cancel,		/* Second label (or 'NULL') */
		const char * alternate )
{
	notice_win *nw;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * hbox1;
	GtkWidget * image;
	nw = &noticeW;
	
	char *aff = NULL;
	char *can = NULL;
	char *alt = NULL;
	
#ifndef GTK1
		nw->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
		/*gtk_window_set_decorated( GTK_WINDOW(nw->win), FALSE );*/
#else
		nw->win = gtk_window_new( GTK_WINDOW_DIALOG );
#endif
	gtk_window_position( GTK_WINDOW(nw->win), GTK_WIN_POS_CENTER );
	gtk_container_set_border_width (GTK_CONTAINER (nw->win), 0);
	gtk_window_set_resizable (GTK_WINDOW (nw->win), FALSE);
	gtk_window_set_modal (GTK_WINDOW (nw->win), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (nw->win), GDK_WINDOW_TYPE_HINT_DIALOG);
		
	vbox = gtk_vbox_new( FALSE, 12 );
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER(nw->win), vbox );
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
		
	hbox = gtk_hbox_new( FALSE, 12 );
	gtk_box_pack_start( GTK_BOX(vbox), hbox, TRUE, TRUE, 0 );
	gtk_widget_show(hbox);

 	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);
  gtk_misc_set_alignment (GTK_MISC (image), 0, 0);

	/* create the text label, allow GTK to wrap and allow for markup (for future enhancements) */
	nw->label = gtk_label_new(msg);
	gtk_widget_show( nw->label );
	gtk_box_pack_end (GTK_BOX (hbox), nw->label, TRUE, TRUE, 0);
	gtk_label_set_use_markup (GTK_LABEL (nw->label), FALSE);
	gtk_label_set_line_wrap (GTK_LABEL (nw->label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (nw->label), 0, 0);

	/* this hbox will include the button bar */
	hbox1 = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, TRUE, 0);

	/* add the respective buttons */
	aff = gtkChgMnemonic( (char *) affirmative);
	nw->butt[ 0 ] = gtk_button_new_with_mnemonic (aff);
	gtk_widget_show (nw->butt[ 0 ]);
	gtk_box_pack_end (GTK_BOX (hbox1), nw->butt[ 0 ], TRUE, TRUE, 0);
 	gtk_container_set_border_width (GTK_CONTAINER (nw->butt[ 0 ]), 3);
	gtk_signal_connect( GTK_OBJECT(nw->butt[0]), "clicked", GTK_SIGNAL_FUNC(doNotice), (void*)1 );
  GTK_WIDGET_SET_FLAGS (nw->butt[ 0 ], GTK_CAN_DEFAULT);

	if( cancel ) {
		can = gtkChgMnemonic( (char *) cancel);
		nw->butt[ 1 ] = gtk_button_new_with_mnemonic (can);
 		gtk_widget_show (nw->butt[ 1 ]);
  	gtk_box_pack_end (GTK_BOX (hbox1), nw->butt[ 1 ], TRUE, TRUE, 0);
	  gtk_container_set_border_width (GTK_CONTAINER (nw->butt[ 1 ]), 3);
		gtk_signal_connect( GTK_OBJECT(nw->butt[1]), "clicked", GTK_SIGNAL_FUNC(doNotice), (void*)0 );
	  GTK_WIDGET_SET_FLAGS (nw->butt[ 1 ], GTK_CAN_DEFAULT);

		if( alternate ) {
			alt = gtkChgMnemonic( (char *) alternate);
			nw->butt[ 2 ] = gtk_button_new_with_mnemonic (alt);
			gtk_widget_show (nw->butt[ 2 ]);
			gtk_box_pack_start (GTK_BOX (hbox1), nw->butt[ 2 ], TRUE, TRUE, 0);
			gtk_container_set_border_width (GTK_CONTAINER (nw->butt[ 2 ]), 3);
			gtk_signal_connect( GTK_OBJECT(nw->butt[2]), "clicked", GTK_SIGNAL_FUNC(doNotice), (void*)-1 );
		  GTK_WIDGET_SET_FLAGS (nw->butt[ 2 ], GTK_CAN_DEFAULT);
 		}
	}	

  gtk_widget_grab_default (nw->butt[ 0 ]);
  gtk_widget_grab_focus (nw->butt[ 0 ]);

	gtk_widget_show( nw->win );

	if ( gtkMainW ) {
		gtk_window_set_transient_for( GTK_WINDOW(nw->win), GTK_WINDOW( gtkMainW->gtkwin) );
/*		gdk_window_set_group( nw->win->window, gtkMainW->gtkwin->window ); */
	}
	gtkDoModal( NULL, TRUE );
	
	if( aff )
		free( aff );
		
	if( can )
		free( can );
		
	if( alt )
		free( alt );		
		
	return noticeValue;
}


EXPORT void wFlush(
		void )
/*
Flushs all commands to the Window.
*/
{
	while ( gtk_events_pending() )
		gtk_main_iteration();

	gdk_display_sync(gdk_display_get_default());
}


void wWinTop( wWin_p win )
{
}


void wSetCursor( wCursor_t cursor )
{
}


const char * wMemStats( void )
{
#ifdef LATER
	static char msg[80];
	struct mstats stats;
	stats = mstats();
	sprintf( msg, "Total %d, used %d(%d), free %d(%d)",
				stats.bytes_total,
				stats.bytes_used, stats.chunks_used,
				stats.bytes_free, stats.chunks_free );
	return msg;
#else
	return "No stats available";
#endif
}


wBool_t wCheckExecutable( void )
{
	return TRUE;
}


void wGetDisplaySize( wPos_t * w, wPos_t * h )
{

	*w = gdk_screen_width();
	*h = gdk_screen_height();
}


wIcon_p wIconCreateBitMap( wPos_t w, wPos_t h, const char * bits, wDrawColor color )
{
	wIcon_p ip;
	ip = (wIcon_p)malloc( sizeof *ip );
	ip->gtkIconType = gtkIcon_bitmap;
	ip->w = w;
	ip->h = h;
	ip->color = color;
	ip->bits = bits;
	return ip;
}

wIcon_p wIconCreatePixMap( char *pm[] )
{
	wIcon_p ip;
	ip = (wIcon_p)malloc( sizeof *ip );
	ip->gtkIconType = gtkIcon_pixmap;
	ip->w = 0;
	ip->h = 0;
	ip->color = 0;
	ip->bits = pm;
	return ip;
}


void wIconSetColor( wIcon_p ip, wDrawColor color )
{
	ip->color = color;
}

void wConvertToCharSet( char * buffPtr, int buffMax )
{
}


void wConvertFromCharSet( char * buffPtr, int buffMax )
{
}

static dynArr_t conversionBuffer_da;
#define convesionBuffer(N) DYNARR_N( char, conversionBuffer_da, N )

char * gtkConvertInput( const char * inString )
{
#ifndef GTK1
	const char * cp;
	char * cq;
	int extCharCnt, inCharCnt;

	/* Already UTF-8 encoded? */
	if (g_utf8_validate(inString, -1, NULL))
		/* Yes, do not double-convert */
		return (char*)inString;
#ifdef VERBOSE
	fprintf(stderr, "gtkConvertInput(%s): Invalid UTF-8, converting...\n", inString);
#endif

	for ( cp=inString, extCharCnt=0; *cp; cp++ ) {
		if ( ((*cp)&0x80) != 0 )
			extCharCnt++;
	}
	inCharCnt = cp-inString;
	if ( extCharCnt == 0 )
		return (char*)inString;
	DYNARR_SET( char, conversionBuffer_da, inCharCnt+extCharCnt+1 );
	for ( cp=inString, cq=(char*)conversionBuffer_da.ptr; *cp; cp++ ) {
		if ( ((*cp)&0x80) != 0 ) {
			*cq++ = 0xC0+(((*cp)&0xC0)>>6);
			*cq++ = 0x80+((*cp)&0x3F);
		} else {
			*cq++ = *cp;
		}
	}
	*cq = 0;
	return (char*)conversionBuffer_da.ptr;
#else
	return (char*)inString;
#endif
}


char * gtkConvertOutput( const char * inString )
{
#ifndef GTK1
	const char * cp;
	char * cq;
	int extCharCnt, inCharCnt;
	for ( cp=inString, extCharCnt=0; *cp; cp++ ) {
		if ( ((*cp)&0xC0) == 0x80 )
			extCharCnt++;
	}
	inCharCnt = cp-inString;
	if ( extCharCnt == 0 )
		return (char*)inString;
	DYNARR_SET( char, conversionBuffer_da, inCharCnt+1 );
	for ( cp=inString, cq=(char*)conversionBuffer_da.ptr; *cp; cp++ ) {
		if ( ((*cp)&0x80) != 0 ) {
			*cq++ = 0xC0+(((*cp)&0xC0)>>6);
			*cq++ = 0x80+((*cp)&0x3F);
		} else {
			*cq++ = *cp;
		}
	}
	*cq = 0;
	return (char*)conversionBuffer_da.ptr;
#else
	return (char*)inString;
#endif
}

/*-----------------------------------------------------------------*/

typedef struct accelData_t {
		wAccelKey_e key;
		int modifier;
		wAccelKeyCallBack_p action;
		void * data;
		} accelData_t;
static dynArr_t accelData_da;
#define accelData(N) DYNARR_N( accelData_t, accelData_da, N )


static guint accelKeyMap[] = {
			0,			/* wAccelKey_None, */
			GDK_Delete,		/* wAccelKey_Del, */
			GDK_Insert,		/* wAccelKey_Ins, */
			GDK_Home,		/* wAccelKey_Home, */
			GDK_End,		/* wAccelKey_End, */
			GDK_Page_Up,	/* wAccelKey_Pgup, */
			GDK_Page_Down,	/* wAccelKey_Pgdn, */
			GDK_Up,		/* wAccelKey_Up, */
			GDK_Down,		/* wAccelKey_Down, */
			GDK_Right,		/* wAccelKey_Right, */
			GDK_Left,		/* wAccelKey_Left, */
			GDK_BackSpace,	/* wAccelKey_Back, */
			GDK_F1,		/* wAccelKey_F1, */
			GDK_F2,		/* wAccelKey_F2, */
			GDK_F3,		/* wAccelKey_F3, */
			GDK_F4,		/* wAccelKey_F4, */
			GDK_F5,		/* wAccelKey_F5, */
			GDK_F6,		/* wAccelKey_F6, */
			GDK_F7,		/* wAccelKey_F7, */
			GDK_F8,		/* wAccelKey_F8, */
			GDK_F9,		/* wAccelKey_F9, */
			GDK_F10,		/* wAccelKey_F10, */
			GDK_F11,		/* wAccelKey_F11, */
			GDK_F12		/* wAccelKey_F12, */
		};


EXPORT void wAttachAccelKey(
		wAccelKey_e key,
		int modifier,
		wAccelKeyCallBack_p action,
		void * data )
{
	accelData_t * ad;
	if ( key < 1 || key > wAccelKey_F12 ) {
		fprintf( stderr, "wAttachAccelKey(%d) out of range\n", (int)key );
		return;
	}
	DYNARR_APPEND( accelData_t, accelData_da, 10 );
	ad = &accelData(accelData_da.cnt-1);
	ad->key = key;
	ad->modifier = modifier;
	ad->action = action;
	ad->data = data;
}


EXPORT struct accelData_t * gtkFindAccelKey(
		GdkEventKey * event )
{
	accelData_t * ad;
	int modifier = 0;
	if ( ( event->state & GDK_SHIFT_MASK ) )
		modifier |= WKEY_SHIFT;
	if ( ( event->state & GDK_CONTROL_MASK ) )
		modifier |= WKEY_CTRL;
	if ( ( event->state & GDK_MOD1_MASK ) )
		modifier |= WKEY_ALT;
	for ( ad=&accelData(0); ad<&accelData(accelData_da.cnt); ad++ )
		if ( event->keyval == accelKeyMap[ad->key] &&
			 modifier == ad->modifier )
			return ad;
	return NULL;
}


EXPORT wBool_t gtkHandleAccelKey(
		GdkEventKey *event )
{
	accelData_t * ad = gtkFindAccelKey( event );
	if ( ad ) {
		ad->action( ad->key, ad->data );
		return TRUE;
	}
	return FALSE;
}

/*
 *****************************************************************************
 *
 * Timer Functions 
 *
 *****************************************************************************
 */

static wBool_t gtkPaused = FALSE;
static int alarmTimer = 0;

static gint doAlarm(
		gpointer data )
{
	wAlarmCallBack_p func = (wAlarmCallBack_p)data;
	if (alarmTimer)
		gtk_timeout_remove( alarmTimer );
	func();
	alarmTimer = 0;
	return 0;
}


EXPORT void wAlarm(
		long count,
		wAlarmCallBack_p func )		/* milliseconds */
/*
Alarm for <count> milliseconds.
*/
{
	gtkPaused = TRUE;
	if (alarmTimer)
		gtk_timeout_remove( alarmTimer );
	alarmTimer = gtk_timeout_add( count, doAlarm, (void *) (GtkFunction)func );
}


static wControl_p triggerControl = NULL;
static setTriggerCallback_p triggerFunc = NULL;

static void doTrigger( void )
{
	if (triggerControl && triggerFunc) {
		triggerFunc( triggerControl );
		triggerFunc = NULL;
		triggerControl = NULL;
	}
}

void gtkSetTrigger(
		wControl_p b,
		setTriggerCallback_p trigger )
{
	triggerControl = b;
	triggerFunc = trigger;
	wAlarm( 500, doTrigger );
}


EXPORT void wPause(
		long count )		/* milliseconds */
/*
Pause for <count> milliseconds.
*/
{
	struct timeval timeout;
	sigset_t signal_mask;
	sigset_t oldsignal_mask;
	
	gdk_display_sync(gdk_display_get_default());
	
	timeout.tv_sec = count/1000;
	timeout.tv_usec = (count%1000)*1000;
	
	sigemptyset( &signal_mask );
	sigaddset( &signal_mask, SIGIO );
	sigaddset( &signal_mask, SIGALRM );	
	sigprocmask( SIG_BLOCK, &signal_mask, &oldsignal_mask );
	
	if (select( 0, NULL, NULL, NULL, &timeout ) == -1) {
		perror("wPause:select");
	}
	sigprocmask( SIG_BLOCK, &oldsignal_mask, NULL );
}


unsigned long wGetTimer( void )
{
	struct timeval tv;
	struct timezone tz;
	int rc;
	rc = gettimeofday( &tv, &tz );
	return (tv.tv_sec-startTime.tv_sec+1) * 1000 + tv.tv_usec /1000;
}



/**
 * Add control to circular list of synonymous controls. Synonymous controls are kept in sync by 
 * calling wControlLinkedActive for one member of the list 
 *
 * \param b1 IN  first control
 * \param b2 IN  second control
 * \return    none
 */
 
EXPORT void wControlLinkedSet( wControl_p b1, wControl_p b2 )
{
	
	b2->synonym = b1->synonym;
	if( b2->synonym == NULL )
		b2->synonym = b1;
		
	b1->synonym = b2;
} 	

/**
 * Activate/deactivate a group of synonymous controls.
 *
 * \param b IN  control
 * \param active IN  state
 * \return    none
 */
 

EXPORT void wControlLinkedActive( wControl_p b, int active )
{
	wControl_p savePtr = b;

	if( savePtr->type == B_MENUITEM )
		wMenuPushEnable( (wMenuPush_p)savePtr, active );
	else 	
		wControlActive( savePtr, active );		

	savePtr = savePtr->synonym;

	while( savePtr && savePtr != b ) {	

		if( savePtr->type == B_MENUITEM )
			wMenuPushEnable( (wMenuPush_p)savePtr, active );
		else 	
			wControlActive( savePtr, active );		

		savePtr = savePtr->synonym;
	}	
}

/*
 *****************************************************************************
 *
 * Control Utilities
 *
 *****************************************************************************
 */

EXPORT void wControlShow(
		wControl_p b,		/* Control */
		wBool_t show )		/* Command */
/*
Cause the control <b> to be displayed or hidden.
Used to hide control (such as a list) while it is being updated.
*/
{
	if ( b->type == B_LINES ) {
		gtkLineShow( (wLine_p)b, show );
		return;
	}
	if (b->widget == 0) abort();
	if (show) {
		gtk_widget_show( b->widget );
		if (b->label)
			gtk_widget_show( b->label );
	} else {
		gtk_widget_hide( b->widget );
		if (b->label)
			gtk_widget_hide( b->label );
	}
}

EXPORT void wControlActive(
		wControl_p b,		/* Control */
		int active )		/* Command */
/*
Cause the control <b> to be marked active or inactive.
Inactive controls donot respond to actions.
*/
{
	if (b->widget == 0) abort();
	gtk_widget_set_sensitive( GTK_WIDGET(b->widget), active );
}


EXPORT wPos_t wLabelWidth(
		const char * label )		/* Label */
/*
Returns the width of <label>.
This is used for computing window layout.
Typically the width to the longest label is computed and used as
the X-position for <controls>.
*/
{
	GtkWidget * widget;
	GtkRequisition requisition;
	widget = gtk_label_new( gtkConvertInput(label) );
	gtk_widget_size_request( widget, &requisition );
	gtk_widget_destroy( widget );
	return requisition.width+8;
}


EXPORT wPos_t wControlGetWidth(
		wControl_p b)		/* Control */
{
	return b->w;
}


EXPORT wPos_t wControlGetHeight(
		wControl_p b)		/* Control */
{
	return b->h;
}


EXPORT wPos_t wControlGetPosX(
		wControl_p b)		/* Control */
{
	return b->realX;
}


EXPORT wPos_t wControlGetPosY(
		wControl_p b)		/* Control */
{
	return b->realY - FOUR - ((b->parent->option&F_MENUBAR)?MENUH:0);
}


EXPORT void wControlSetPos(
		wControl_p b,		/* Control */
		wPos_t x,		/* X-position */
		wPos_t y )		/* Y-position */
{
	b->realX = x;
	b->realY = y + FOUR + ((b->parent->option&F_MENUBAR)?MENUH:0);
#ifndef GTK1
	if (b->widget)
		gtk_fixed_move( GTK_FIXED(b->parent->widget), b->widget, b->realX, b->realY );
	if (b->label)
		gtk_fixed_move( GTK_FIXED(b->parent->widget), b->label, b->realX-b->labelW, b->realY+LABEL_OFFSET );
#else
	if (b->widget)
		gtk_widget_set_uposition( b->widget, b->realX, b->realY );
	if (b->label)
		gtk_widget_set_uposition( b->label, b->realX-b->labelW, b->realY+LABEL_OFFSET );
#endif
}


EXPORT void wControlSetLabel(
		wControl_p b,
		const char * labelStr )
{
	GtkRequisition requisition;
	if (b->label) {
		gtk_label_set( GTK_LABEL(b->label), gtkConvertInput(labelStr) );
		gtk_widget_size_request( b->label, &requisition );
		b->labelW = requisition.width+8;
#ifndef GTK1
		gtk_fixed_move( GTK_FIXED(b->parent->widget), b->label, b->realX-b->labelW, b->realY+LABEL_OFFSET );
#else
		gtk_widget_set_uposition( b->label, b->realX-b->labelW, b->realY+LABEL_OFFSET );
#endif
	} else {
		b->labelW = gtkAddLabel( b, labelStr );
	}
}

EXPORT void wControlSetContext(
		wControl_p b,
		void * context )
{
	b->data = context;
}


EXPORT void wControlSetFocus(
		wControl_p b )
{
}


static int gtkControlHiliteWidth = 3;
EXPORT void wControlHilite(
		wControl_p b,
		wBool_t hilite )
{
	int off = gtkControlHiliteWidth/2+1;
	if ( b->parent->gc == NULL ) {
		b->parent->gc = gdk_gc_new( b->parent->gtkwin->window );
		gdk_gc_copy( b->parent->gc, b->parent->gtkwin->style->base_gc[GTK_STATE_NORMAL] );
		b->parent->gc_linewidth = 0;
		gdk_gc_set_line_attributes( b->parent->gc, b->parent->gc_linewidth, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER );
	}
	if ( b->widget == NULL )
		return;
	if ( ! GTK_WIDGET_VISIBLE( b->widget ) )
		return;
	if ( ! GTK_WIDGET_VISIBLE( b->parent->widget ) )
		return;
	gdk_gc_set_foreground( b->parent->gc, gtkGetColor( wDrawColorBlack, FALSE ) );
	gdk_gc_set_function( b->parent->gc, GDK_XOR );
	gdk_gc_set_line_attributes( b->parent->gc, gtkControlHiliteWidth, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER );
	gdk_draw_line( b->parent->widget->window, b->parent->gc,
		b->realX - gtkControlHiliteWidth,
		b->realY - off,
		b->realX + b->w + gtkControlHiliteWidth,
		b->realY - off );
	gdk_draw_line( b->parent->widget->window, b->parent->gc,
		b->realX - gtkControlHiliteWidth,
		b->realY + b->h + off - 1,
		b->realX + b->w + gtkControlHiliteWidth,
		b->realY + b->h + off - 1 );
	gdk_draw_line( b->parent->widget->window, b->parent->gc,
		b->realX - off,
		b->realY,
		b->realX - off,
		b->realY + b->h );
	gdk_draw_line( b->parent->widget->window, b->parent->gc,
		b->realX + b->w + off - 1,
		b->realY,
		b->realX + b->w + off - 1,
		b->realY + b->h );
}

/*
 *******************************************************************************
 *
 * Main
 *
 *******************************************************************************
 */

#ifdef GTK
static wBool_t wAbortOnErrors = FALSE;
#endif

int do_rgb_init = 1;

int main( int argc, char *argv[] )
{
	wWin_p win;
	wControl_p b;
	const char *ld, *hp;
	static char buff[BUFSIZ];

	if ( getenv( "GTKLIB_NOLOCALE" ) == 0 )
		setlocale( LC_ALL, "en_US" );
	gtk_init( &argc, &argv );
	gettimeofday( &startTime, NULL );

	if ( getenv( "XVLIB_REVERSEICON" ) != 0 )
		reverseIcon = !reverseIcon;

	if ( do_rgb_init )
		gdk_rgb_init();	/* before we try to draw */

	if ((win=wMain( argc, argv )) == (wWin_p)0)
		exit(1);

	ld = wGetAppLibDir();
	if (ld != NULL) {
		sprintf( buff, "HELPPATH=/usr/lib/help:%s:", ld );
		if ( (hp = getenv("HELPPATH")) != NULL )
			strcat( buff, hp );
		putenv( buff );
	}

	if (!win->shown)
		wWinShow( win, TRUE );
	for (b=win->first; b != NULL; b = b->next) {
		if (b->repaintProc)
			b->repaintProc( b );
	}
	gtk_main();
	exit(0);
}
