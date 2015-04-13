/** \file gtkxpm.c
 * XPM creation functions
 */

/*  XTrackCad - Model Railroad CAD
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
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>

#include <gtk/gtk.h>
#include "gtkint.h"

 #include "uthash.h"

struct xpmColTable {
		int color; 	          	/* color value (rgb) */
		char name[ 5 ];       	/* corresponding character representation */      
		UT_hash_handle hh; 	/* makes this structure hashable */
};

static struct xpmColTable *colTable = NULL;

// must be 64 chars long
static char colVal[] = ".*0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
 

struct wDraw_t {
		WOBJ_COMMON
		void * context;
		wDrawActionCallBack_p action;
		wDrawRedrawCallBack_p redraw;

		GdkPixmap * pixmap;
		GdkPixmap * pixmapBackup;

		double dpi;

		GdkGC * gc;
		wDrawWidth lineWidth;
		wDrawOpts opts;
		wPos_t maxW;
		wPos_t maxH;
		unsigned long lastColor;
		wBool_t lastColorInverted;
		const char * helpStr;

		wPos_t lastX;
		wPos_t lastY;

		wBool_t delayUpdate;
		};
		
 /**
 * Export as XPM bitmap file. During creation of the color table, a 4 byte color
 * encoding is assumed and a table created accordingly. Once the whole picture has been scanned
 * the correct number ist known. When writing to disk only the needed number of bytes per entry
 * is written.
 * This routine was heavily inspired by on implementation for TK written by Jan Nijtmans.
 *
 * \param d IN the drawing area ?
 * \param fileName IN  fully qualified filename for the bitmap file. 
 * \return    TRUE on success, FALSE on error
 */

wBool_t wBitMapWriteFile(       wDraw_p d, const char * fileName )
{
	GdkImage * image;
	gint x, y;
	guint32 pixel;
	FILE * f;
	int cc = 0; 
	struct xpmColTable	 *ct,  *tmp;
	int numChars;

	image = gdk_image_get( (GdkWindow*)d->pixmap, 0, 0, d->w, d->h );
	if (!image) {
		wNoticeEx( NT_ERROR, "WriteBitMap: image_get failed", "Ok", NULL );
		return FALSE;
	}

	f = fopen( fileName, "w" );
	if (!f) {
		perror( fileName );
		return FALSE;
	}
	fprintf( f, "/* XPM */\n" );
	fprintf( f, "static char * xtrkcad_bitmap[] = {\n" );
	fprintf( f, "/* width height num_colors chars_per_pixel */\n" );
	
	// count colors used and create the color table in the same pass
	for( y = 0; y < d->h;y ++ ) {
		for (x = 0; x < d->w; x++ ) {
			
			pixel = gdk_image_get_pixel( image, x, y );
			//check whether color is new

			HASH_FIND(hh, colTable, &pixel, sizeof( guint32 ), ct);
			if( !ct ) {
				// not found previously, so add a new color table entry
				int i;
				int c;
				
				ct = malloc( sizeof( struct xpmColTable ) );
				ct->name[ 4 ] = '\0';
				for( i = 3, c = cc; i >= 0; i--, c>>=6 ) {
					(ct->name)[ i ] = colVal[ c &  0x3F ];
				}	
				ct->color = pixel;
				
				HASH_ADD(hh, colTable, color, sizeof( guint32 ), ct);
				cc++;
			}	
		}	
	}

	// calculate how many characters are needed for the color table
	numChars = 1;
	if( cc > 0x3ffff ) {
		numChars = 4;
	} else {
		if( cc > 0xfff ) {
			numChars = 3;
		} else {
			if( cc > 0x3f ) {
				numChars = 2;
			} 	
		}
	}	
	// print color table 
	fprintf( f, "\"%d %d %d %d\"\n", d->w, d->h, cc, numChars );
	fprintf( f, "/* colors */\n" );
	for( ct = colTable; ct != NULL; ct = ct->hh.next )
		fprintf( f, "\"%s c #%6.6x\",\n", (ct->name) + (4 - numChars ), ct->color );
	
	// print the pixels
	fprintf( f, "/* pixels */\n" );
	for ( y=0; y<d->h; y++ ) {
		fprintf( f, "\"" );
		for ( x=0; x<d->w; x++ ) {
			pixel = gdk_image_get_pixel( image, x, y );
			HASH_FIND( hh, colTable, &pixel, sizeof(guint32), ct );
			fputs( (ct->name) + (4 - numChars ), f ); 
		}
		fprintf( f, "\"%s\n", (y<d->h-1)?",":"" );
	}

	// delete the hash and free the content
	HASH_ITER(hh, colTable, ct, tmp) {
		HASH_DEL(colTable,ct);  		
		free(ct);           
	}
	
	gdk_image_destroy( image );
	fprintf( f, "};\n" );
	fclose( f );
	return TRUE;
}
