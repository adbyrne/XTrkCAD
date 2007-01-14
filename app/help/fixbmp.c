
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

#define TRUE	(1)
#define FALSE	(0)

struct BITMAPFILEHEADER {
	char bfType[2];
	long bfSize;
	short bfRsvd1;
	short bfRsvd2;
	long bfOffBits;
	};

struct BITMAPINFOHEADER {
	long biSize;
	long biWidth;
	long biHeight;
	short biPlanes;
	short biBitCount;
	long biCompression;
	long biSizeImage; 
	long biXPelsPerMeter;
	long biYPelsPerMeter;
	long biClrUsed;
	long biClrImportant; 
	};

int namenames;
int dumpInfo;
int dumpColorMap;
int dumpBits;
int dumpHisto;
int updateColorCount;
int zeroColorCount;
int setColorCount;
int fixColorMap;

int fixbmp( char * filename )
{
	FILE * file;
	struct BITMAPFILEHEADER bmfh;
	struct BITMAPINFOHEADER bmih;
	int rc;
	long colors[256];
	int i, j, size;
	unsigned char * bmp, bit;
	int colorCnt;
	long maxcolor;
	long histo[256];;

	if ( namenames )
		printf( "%s\n", filename );
	file = fopen( filename, "r+" );
	if ( file == NULL ) {
		fprintf( stderr, "%s: Cant open:%s\n", filename, filename );
		return FALSE;
	}
	rc = fread( &bmfh.bfType, 1, sizeof bmfh.bfType, file );
	rc += fread( &bmfh.bfSize, 1, sizeof bmfh.bfSize, file );
	rc += fread( &bmfh.bfRsvd1, 1, sizeof bmfh.bfRsvd1, file );
	rc += fread( &bmfh.bfRsvd2, 1, sizeof bmfh.bfRsvd2, file );
	rc += fread( &bmfh.bfOffBits, 1, sizeof bmfh.bfOffBits, file );
	if ( rc != 14 ) {
		fprintf( stderr, "%s: Bad read of bmfh: %d\n", filename, rc );
		return FALSE;
	}
	rc = fread( &bmih, 1, sizeof bmih, file );
	if ( rc != sizeof bmih ) {
		fprintf( stderr, "%s: Bad read of bmih: %d\n", filename, rc );
		return FALSE;
	}
	if ( dumpInfo ) {
		printf( "fh:sz=%d, off=%ld\n", bmfh.bfSize, bmfh.bfOffBits );
		printf( "ih:sz=%ld, w=%ld, h=%ld, (%ld), pl=%d, bc=%d, co=%ld, si=%ld, cu=%ld, ci=%ld\n",
			bmih.biSize, bmih.biWidth, bmih.biHeight, bmih.biWidth*bmih.biHeight,
			bmih.biPlanes, bmih.biBitCount, bmih.biCompression,
			bmih.biSizeImage, bmih.biClrUsed, bmih.biClrImportant );
	}
	if ( bmih.biPlanes != 1 || bmih.biBitCount != 8 ) {
		fprintf( stderr, "%s: bad Planes(%d) or BitCount(%d)\n", filename, bmih.biPlanes, bmih.biBitCount );
		return FALSE;
	}
	if ( bmih.biClrUsed > 256 ) {
		fprintf( stderr, "%s: Too many colors (%ld)\n", filename, bmih.biClrUsed );
		return FALSE;
	}
	colorCnt = bmih.biClrUsed;
	if ( colorCnt == 0 )
		colorCnt = 256;
	rc = fread( colors, sizeof colors[0], colorCnt, file );
	if ( rc != colorCnt ) {
		fprintf( stderr, "%s: Bad read of colors: %d\n", filename, rc );
		return FALSE;
	}
	if ( dumpColorMap ) {
		printf( "colorcnt=%d", rc );
		for ( i=0; i<colorCnt; i++ ) {
			if ( i%8 == 0 )
				printf( "\n%2.2x: ", i );
			printf( "%8.8lx ", colors[i] );
		}
		printf( "\n" );
	}
	if ( fixColorMap ) {
		long c;
		for ( i=0; i<colorCnt; i++ ) {
			c = colors[i]&0xFFFFFF;
			if ( (c & 0xFF0000) != 0xFF0000 )
				c &= 0xF0FFFF;
			if ( (c & 0x00FF00) != 0x00FF00 )
				c &= 0xFFF0FF;
			if ( (c & 0x0000FF) != 0x0000FF )
				c &= 0xFFFFF0;
			colors[i] = c;
		}
		fseek( file, 14+40, SEEK_SET );
		rc = fwrite( colors, sizeof colors[0], colorCnt, file );
		if ( rc != colorCnt ) {
			fprintf( stderr, "%s: Bad write of colors: %d\n", filename, rc );
			return FALSE;
		}
	}
	size = (int)(bmih.biWidth*bmih.biHeight);
	size = (int)bmih.biWidth;
	size = (size+3)/4*4;
	fseek( file, bmfh.bfOffBits, SEEK_SET );
	bmp = (unsigned char*)malloc( size );
	if ( bmp == NULL ) {
		fprintf( stderr, "%s: Cant malloc(%d) for bitmap\n", filename, size );
		return FALSE;
	}
	maxcolor = 0;
	memset( histo, 0, sizeof histo );
	for ( j=0; j<bmih.biHeight; j++ ) {
		rc = fread( bmp, 1, size, file );
		if ( rc != size ) {
			fprintf( stderr, "%s: Cant read bits for line %d: %d\n", filename, j, rc );
			return FALSE;
		}
		if ( dumpBits )
			printf( "%2.2d: ", j );
		for ( i=0; i<bmih.biWidth; i++ ) {
			bit = bmp[i];
			histo[bit]++;
			if ( dumpBits )
				printf( "%2.2x", bit );
			if ( bit > maxcolor )
				maxcolor = bit;
		}
		if ( dumpBits )
			printf( "\n" );
	}
	free( bmp );
	if ( dumpHisto ) {
		printf( "maxcolor=%ld\n", maxcolor );
		for ( i=0; i<256; i++ )
			if ( histo[i] )
				printf( "[%2.2x]%8.8x = %ld\n", i, colors[i], histo[i] );
	
	}
	if ( updateColorCount || zeroColorCount || setColorCount ) {
		fseek( file, 14, SEEK_SET );
		if ( updateColorCount ) {
			bmih.biClrImportant = maxcolor;
		} else if ( zeroColorCount ) {
			bmih.biClrImportant = 0;
			bmih.biClrUsed = 0;
		} else {
			bmih.biClrImportant = 256;
			bmih.biClrUsed = 256;
		}
		rc = fwrite( &bmih, 1, sizeof bmih, file );
		if ( rc != sizeof bmih ) {
			fprintf( stderr, "%s: Update failed; %d\n", filename, rc );
		}
	}
	fclose( file );
	return TRUE;
}


int main( int argc, char * argv[] )
{
	while ( argc > 2 && argv[1][0] == '-' ) {
		switch ( argv[1][1] ) {
		case 'a': dumpInfo++; dumpColorMap++; dumpBits++; dumpHisto++; break;
		case 'i': dumpInfo++; break;
		case 'c': dumpColorMap++; break;
		case 'b': dumpBits++; break;
		case 'h': dumpHisto++; break;
		case 'u': updateColorCount++; break;
		case 'z': zeroColorCount++; break;
		case 's': setColorCount++; break;
		case 'f': fixColorMap++; break;
		default:
			fprintf( stderr, "bad option %s\n", argv[1] );
		}
		argc--;
		argv++;
	}
	if ( argc > 2 && dumpInfo+dumpColorMap+dumpBits+dumpHisto > 0 )
		namenames++;
	while ( argc > 1 ) {
		fixbmp( argv[1] );
		argc--;
		argv++;
	}
}
