/** \file mswbitmap.c
 * Bitmap handling functions
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/mswlib/mswbitmap.c,v 1.1 2009-09-20 14:55:54 m_fischer Exp $
 */
/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2009 Martin Fischer
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

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <stdio.h>
#include <assert.h>
#include "mswint.h"
#include "i18n.h"

#if _MSC_VER > 1300
	#define stricmp _stricmp
	#define strnicmp _strnicmp
	#define strdup _strdup
#endif

struct wBitmap_t {
		WOBJ_COMMON
		};

HPALETTE hOldPal;

HBITMAP mswCreateBitMap(
		COLORREF fgCol1,
		COLORREF fgCol2,
		COLORREF bgCol,
		wPos_t w,
		wPos_t h,
		const char * bits )
{
	HDC hDc;
	HDC hButtDc;
	HBRUSH oldBrush, newBrush;
	RECT rect;
	HBITMAP hBitMap;
	HBITMAP hOldBitMap;
	const char * byts_p;
	int byt, i, j;

	hDc = GetDC( mswHWnd );
	hButtDc = CreateCompatibleDC( hDc );
	hBitMap = CreateCompatibleBitmap( hDc, w, h );
	ReleaseDC( mswHWnd, hDc );
	hOldBitMap = SelectObject( hButtDc, hBitMap );
	if (mswPalette) {
		hOldPal = SelectPalette( hButtDc, mswPalette, 0 );
	}
	
	/*PatBlt( hButtDc, 0, 0, w, h, WHITENESS );*/
	newBrush = CreateSolidBrush( bgCol );
	oldBrush = SelectObject( hButtDc, newBrush );
	rect.top = 0;
	rect.left = 0;
	rect.bottom = h;
	rect.right = w;
	FillRect( hButtDc, &rect, newBrush );
	DeleteObject( SelectObject( hButtDc, oldBrush ) );

	byts_p = bits;
	for ( j = 0; j < h; j++ ) {
		byt = (0xFF & *byts_p++) | 0x100;
		for ( i = 0; i < w; i++ ) {
			if (byt == 1)
				byt = (0xFF & *byts_p++) | 0x100;
			if ( byt & 0x1 ) {
				SetPixel( hButtDc, i, j, fgCol1 );
				SetPixel( hButtDc, i+1, j+1, fgCol2 );
			}
			byt >>= 1;
		}
	}

	SelectObject( hButtDc, hOldBitMap );
	DeleteDC( hButtDc );
	return hBitMap;
}

dynArr_t bitmap_da;
#define controlMap(N) DYNARR_N(controlMap_t,controlMap_da,N)
#define bitmap(N) DYNARR_N(HBITMAP,bitmap_da,N)

void mswRegisterBitMap(
		HBITMAP hBm )
{
	DYNARR_APPEND( HBITMAP, bitmap_da, 10 );
	bitmap(bitmap_da.cnt-1) = hBm;
}

void deleteBitmaps( void )
{
	int inx;
	for ( inx=0; inx<bitmap_da.cnt; inx++ )
		DeleteObject( bitmap(inx) );
}

/**
 * Draw a bitmap to the screen.
 *
 * \param hDc IN device context 
 * \param offw IN horizontal offset
 * \param offh IN vertical offset
 * \param bm IN icon to draw
 * \param disabled IN draw in disabled state
 * \param color1 IN for two color bitmaps: foreground color enabled state
 * \param color2 IN for two color bitmaps: foreground color disabled state
 *
 */

void mswDrawIcon(
		HDC hDc,
		int offw,
		int offh,
		wIcon_p bm,
		int disabled,
		COLORREF color1,
		COLORREF color2 )
{
	int i;
	int byt;
	BITMAPINFO *bmiInfo;
	COLORREF col;

    /* draw the bitmap by dynamically creating a Windows DIB in memory */
	/* BITMAPINFO already has one RGBQUAD color struct, so only allocate the rest */
    bmiInfo = malloc( sizeof( BITMAPINFO ) + (bm->colorcnt - 1) * sizeof( RGBQUAD ));
    if( !bmiInfo ) {
        fprintf( stderr, "could not allocate memory for bmiInfo\n" );
        abort();
    }

    /* initialize bitmap header from XPM information */
    bmiInfo->bmiHeader.biSize = sizeof( bmiInfo->bmiHeader );
    bmiInfo->bmiHeader.biWidth = bm->w;
    bmiInfo->bmiHeader.biHeight = bm->h;
    bmiInfo->bmiHeader.biPlanes = 1;
    if( bm->type == mswIcon_bitmap )
        bmiInfo->bmiHeader.biBitCount = 1;
    else 
        bmiInfo->bmiHeader.biBitCount = 8;							/* up to 256 colors */
    bmiInfo->bmiHeader.biCompression = BI_RGB;						/* no compression */
    bmiInfo->bmiHeader.biSizeImage = 0;
    bmiInfo->bmiHeader.biXPelsPerMeter = 0;
    bmiInfo->bmiHeader.biYPelsPerMeter = 0;
    bmiInfo->bmiHeader.biClrUsed = bm->colorcnt;					/* number of colors used */
    bmiInfo->bmiHeader.biClrImportant = bm->colorcnt;

	/*
	 * create a transparency mask and paint to screen
	 */ 
	if( bm->type == mswIcon_bitmap ) {
		memset( &bmiInfo->bmiColors[ 0 ], 0xFF, sizeof( RGBQUAD ));
		memset( &bmiInfo->bmiColors[ 1 ], 0, sizeof( RGBQUAD ));
	} else {
		memset( bmiInfo->bmiColors, 0, bm->colorcnt * sizeof( RGBQUAD ));
		memset( &bmiInfo->bmiColors[ bm->transparent ], 0xFF, sizeof( RGBQUAD ));
	}
	StretchDIBits(hDc, offw, offh,
        bmiInfo->bmiHeader.biWidth,
        bmiInfo->bmiHeader.biHeight,
        0, 0,
        bmiInfo->bmiHeader.biWidth,
        bmiInfo->bmiHeader.biHeight,
        bm->pixels, bmiInfo, 				
        DIB_RGB_COLORS, SRCAND);
	
	/* now paint the bitmap with transparent set to black */
	if( bm->type == mswIcon_bitmap ) {
		if( disabled ) {   
			col = color2;
		} else {
			col = color1;
		}
		memset( &bmiInfo->bmiColors[ 0 ], 0, sizeof( RGBQUAD ));
                bmiInfo->bmiColors[ 1 ].rgbRed = GetRValue( col );
                bmiInfo->bmiColors[ 1 ].rgbGreen = GetGValue( col );
                bmiInfo->bmiColors[ 1 ].rgbBlue = GetBValue( col );
    } else {
		if( disabled ) {
			/* create a gray scale palette */
			for( i = 0; i < bm->colorcnt; i ++ ) {
				if (i != bm->transparent) {
					byt = (30 * bm->colormap[i].rgbRed +
						59 * bm->colormap[i].rgbGreen +
						11 * bm->colormap[i].rgbBlue) / 100;

					/* if totally black, use a dark gray */
					if (byt == 0)
						byt = 0x66;

					bmiInfo->bmiColors[i].rgbRed = byt;
					bmiInfo->bmiColors[i].rgbGreen = byt;
					bmiInfo->bmiColors[i].rgbBlue = byt;
				}
			}
	    } else {
            /* copy the palette */
            memcpy( (void *)bmiInfo->bmiColors, (void *)bm->colormap, bm->colorcnt * sizeof( RGBQUAD ));
        }
		memset( &bmiInfo->bmiColors[ bm->transparent ], 0, sizeof( RGBQUAD ));
    }
    
    /* show the bitmap */
    StretchDIBits(hDc, offw, offh,
            bmiInfo->bmiHeader.biWidth,
            bmiInfo->bmiHeader.biHeight,
            0, 0,
            bmiInfo->bmiHeader.biWidth,
            bmiInfo->bmiHeader.biHeight,
            bm->pixels, bmiInfo, 				
            DIB_RGB_COLORS, SRCPAINT);

    /* forget the data */
    free( bmiInfo );
}

/**
 * Create a two color bitmap. This creates a two color icon. Pixels set to 1 are painted 
 * in the specified color, pixels set to 0 are transparent
 * in order to convert the format, a lot of bit fiddling is necessary. The order of 
 * scanlines needs to be reversed and the bit order (high order - low order) is reversed 
 * as well.
 * \param w IN width in pixels
 * \param h IN height in pixels
 * \param bits IN pixel data
 * \param color IN color for foreground
 * \return    pointer to icon
 */

wIcon_p wIconCreateBitMap( wPos_t w, wPos_t h, const char * bits, wDrawColor color )
{
	int lineLength;
	int i, j;
	unsigned char *dest;
	static unsigned char revbits[] = { 0, 0x08, 0x04, 0x0C, 0x02, 0x0A, 0x06, 0x0E, 0x01, 0x09, 0x05, 0x0D, 0x03, 0x0B, 0x07, 0x0F };  
	unsigned long col = wDrawGetRGB( color );

	wIcon_p ip;
	ip = (wIcon_p)malloc( sizeof *ip );
	if( !ip ) {
		fprintf( stderr, "Couldn't allocate memory for bitmap header.\n" );
		abort();
	}

	memset( ip, 0, sizeof *ip );
	ip->type = mswIcon_bitmap;
	ip->w = w;
	ip->h = h;
	ip->colorcnt = 2;

	/* set up our two color palette */
	ip->colormap = malloc( 2 * sizeof( RGBQUAD ));

	ip->colormap[ 1 ].rgbBlue = col & 0xFF;
	ip->colormap[ 1 ].rgbRed = (col>>16) & 0xFF;
	ip->colormap[ 1 ].rgbGreen = (col>>8) & 0xFF;
	ip->colormap[ 1 ].rgbReserved = 0;	

	color = GetSysColor( COLOR_BTNFACE );
	ip->colormap[ 0 ].rgbBlue = GetBValue( color );
	ip->colormap[ 0 ].rgbRed = GetRValue( color );
	ip->colormap[ 0 ].rgbGreen = GetGValue( color );
	ip->colormap[ 0 ].rgbReserved = 0;	

	lineLength = (((( ip->w + 7 ) / 8 ) + 3 ) >> 2 ) << 2;
	ip->pixels = malloc( lineLength * ip->h );
	if( !ip->pixels ) {
		fprintf( stderr, "Couldn't allocate memory for pixel data.\n" );
		abort();
	}

	/* 
	 * copy the bits from source to the buffer, at this time the order of
	 * scanlines is reversed by starting with the last source line.
	 */
	for( i = 0; i < ip->h; i++ ) {
		dest = ip->pixels + i * lineLength;
		memcpy( dest, bits + ( ip->h - i - 1 ) * (( ip->w + 7) / 8), ( ip->w + 7 ) / 8 );

		/*
		 * and now, the bit order is changed, this is done via a lookup table
		 */
		for( j = 0; j < lineLength; j++ )
		{
			unsigned byte = dest[ j ];
			unsigned low = byte & 0x0F;
			unsigned high = (byte & 0xF0) >> 4;
			dest[ j ] = revbits[ low ]<<4 | revbits[ high ];
		}
	}

	return ip;
}

/**
 * Create a pixmap. This functions interprets a XPM icon contained in a
 * char array. Supported format are one or two byte per pixel and #rrggbb
 * or #rrrrggggbbbb color specification. Color 'None' is interpreted as
 * transparency, other symbolic names are not supported.
 *
 * \param pm IN XPM variable
 * \return    pointer to icon, call free() if not needed anymore. 
 */

wIcon_p wIconCreatePixMap( char *pm[])
{
	wIcon_p ip;
	int col, r, g, b, len;
	int width, height;
	char buff[3];
	char * cp, * cq, * ptr;
	int i, j, k;
	int lineLength;
	unsigned *keys;
	unsigned numchars;
	unsigned pixel;

	ip = (wIcon_p)malloc( sizeof *ip );
	if( !ip ) {
		fprintf( stderr, "Couldn't allocate memory for bitmap header.\n" );
		abort();
	}

	memset( ip, 0, sizeof *ip );
	ip->type = mswIcon_pixmap;

	/* extract values */
	cp = pm[0];
	width = (int)strtol(cp, &cq, 10 );			/* width of image */
	height = (int)strtol(cq, &cq, 10 );			/* height of image */
	col = (int)strtol(cq, &cq, 10 );			/* number of colors used */
	numchars = (int)strtol(cq, &cq, 10 );		/* get number of chars per pixel */
	
	ip->colormap = malloc( col * sizeof( RGBQUAD ));
	ip->w = width;	
	ip->h = height;
	ip->colorcnt = col;								/* number of colors used */

	keys = malloc( sizeof( unsigned ) * col );

	for ( col=0; col<(int)ip->colorcnt; col++ ) {
		ptr = strdup( pm[col+1] );				/* create duplicate for input string*/

		if( numchars == 1 ) {
			keys[ col ] = (unsigned)ptr[0];
		}
		else if( numchars == 2 ) {
				keys[ col ] = (unsigned) ( ptr[ 0 ] + ptr[ 1 ] * 256 );
		}
		
		cp = strtok( ptr + numchars, "\t " );	/* cp points to color type */
		assert( *cp == 'c' );					/* should always be color */
		
		cp = strtok( NULL, "\t " );				/* go to next token, the color definition itself */

		if( *cp == '#' ) {						/* is this a hex RGB specification? */
			len = strlen( cp+1 ) / 3;
			assert( len == 4 || len == 2 );		/* expecting three 2 char or 4 char values */	
			buff[2] = 0;						/* if yes, extract the values */
			memcpy( buff, cp + 1, 2 );
			r = (int)strtol(buff, &cq, 16);
			memcpy( buff, cp + 1 + len, 2 );
			g = (int)strtol(buff, &cq, 16);
			memcpy( buff, cp + 1 + 2 * len, 2 );
			b = (int)strtol(buff, &cq, 16);

			ip->colormap[ col ].rgbBlue = b;
			ip->colormap[ col ].rgbGreen = g;
			ip->colormap[ col ].rgbRed = r;
			ip->colormap[ col ].rgbReserved = 0;

		} else {
			if( !stricmp( cp, "none" )) {			/* special case transparency*/
				ip->transparent = col;
			}
			else 
				assert( *cp == '#' );				/* if no, abort for the moment */
		}
		free( ptr );
	}

	/* get memory for the pixel data */
	/* dword align begin of line */
	lineLength = ((ip->w + 3 ) >> 2 ) << 2;
	ip->pixels = malloc( lineLength * ip->h );
	if( !ip->pixels ) {
		fprintf( stderr, "Couldn't allocate memory for pixel data.\n" );
		abort();
	}

	/* 
	   convert the XPM pixel data to indexes into color table
	   at the same time the order of rows is reversed 
	   Win32 should be able to do that but I couldn't find out
	   how, so this is coded by hand. 
	*/

	/* for all rows */
	for( i = 0; i < ip->h; i++ ) {
		
		cq = ip->pixels + lineLength * i;
		/* get the next row */
		cp = pm[ ip->h - i + ip->colorcnt ];
		/* for all pixels in row */
		for( j = 0; j < ip->w; j++ ) {
			/* get the pixel info */
			if( numchars == 1 )
				pixel = ( unsigned )*cp;
			else
				pixel = (unsigned) (*cp + *(cp+1)*256);
			cp += numchars;

			/* look up pixel info in color table */
			k = 0;
			while( pixel != keys[ k ] )
				k++;

			/* save the index into color table */
			*(cq + j) = k;
		}
	}		
	free( keys );
		
	return ip;
}

void wIconSetColor( wIcon_p ip, wDrawColor color )
{
	unsigned long col = wDrawGetRGB( color );

	if( ip->type == mswIcon_bitmap ) {
		ip->colormap[ 1 ].rgbBlue = col & 0xFF;
		ip->colormap[ 1 ].rgbRed = (col>>16) & 0xFF;
		ip->colormap[ 1 ].rgbGreen = (col>>8) & 0xFF;
	}
}

/**
 * Draw icon to screen.
 *
 * \param d IN drawing area
 * \param bm IN bitmap to draw
 * \param x IN x position 
 * \param y IN y position
 */

void
wIconDraw( wDraw_p d, wIcon_p bm, wPos_t x, wPos_t y )
{
	mswDrawIcon( d->hDc, (int)x, (int)y, bm, FALSE, 0, 0 );
}

/**
 * Create a static control for displaying a bitmap.
 *
 * \param parent IN parent window
 * \param x, y   IN position in parent window
 * \param option IN ignored for now
 * \param iconP  IN icon to use
 * \return    the control
 */

wControl_p
wBitmapCreate( wWin_p parent, wPos_t x, wPos_t y, long option, wIcon_p iconP )
{
	wBitmap_p control;
	int index;
	DWORD style = SS_OWNERDRAW | WS_VISIBLE | WS_CHILD;

	control = mswAlloc( parent, B_BITMAP, NULL, sizeof( struct wBitmap_t ), NULL, &index );
	mswComputePos( (wControl_p)control, x, y );
	control->option = option;

	control->hWnd = CreateWindow( "STATIC", NULL,
						style, control->x, control->y,
						iconP->w, iconP->h,
						((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );

	if (control->hWnd == NULL) {
		mswFail("CreateWindow(BITMAP)");
		return (wControl_p)control;
	}
	control->h = iconP->h;
	control->w = iconP->w;
	control->data = iconP;

	return (wControl_p)control;
}