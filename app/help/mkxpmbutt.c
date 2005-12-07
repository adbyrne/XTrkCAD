
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
#include <string.h>

typedef struct {
	int w;
	int h;
	char * b;
	} bitmap_t;

#define RGB(R,G,B) (((R)<<16)|((G)<<8)|(B))

static struct {
	long rgb;
	int key;
	} mainColorMap[256];
static int mainColorCnt;
static int fileColorMap[256];

static int sizeColorMap( void )
{
	return mainColorCnt;
}

static void printColorMap( FILE * fout )
{
	int inx, r, g, b;
	long rgb;
	for ( inx=0; inx<mainColorCnt; inx++ ) {
		rgb = mainColorMap[inx].rgb;
		r = (rgb>>16)&0xFF;
		g = (rgb>>8)&0xFF;
		b = (rgb)&0xFF;
		fprintf( fout, "\"%c\tc #%2.2X%s%2.2X%s%2.2X%s\",\n",
			mainColorMap[inx].key,
			r, ((r&1)?"FF":"00"),
			g, ((g&1)?"FF":"00"),
			b, ((b&1)?"FF":"00") );
	}
}

static int allocColor( long color )
{
	int inx;
	for ( inx=0; inx<mainColorCnt; inx++ ) {
		if ( mainColorMap[inx].rgb == color )
			return mainColorMap[inx].key;
	}
	if ( mainColorCnt >= 256 ) {
		fprintf( stderr, "too many colors\n" );
		exit(1);
	}
	mainColorMap[mainColorCnt].rgb = color;
	mainColorMap[mainColorCnt].key = mainColorCnt+'0';
	return mainColorMap[mainColorCnt++].key;
}

static void resetColor( void )
{
	int inx;
	for ( inx=0; inx<256; inx++ )
		fileColorMap[inx] = -1;
}

static void mapColor( int inx, long rgb )
{
	fileColorMap[inx] = allocColor(rgb);
}

static int remapColor( int oldColor )
{
	int newColor;
	newColor = fileColorMap[oldColor];
	if ( newColor < 0 ) {
		fprintf( stderr, "unknown color inx: %d\n", oldColor );
		return ' ';
	}
	return newColor;
}

static int read_xpm(
	char * filename,
	int *rw,
	int *rh,
	int bx,
	int by,
	bitmap_t *bm )
{
	char line[1024], *cp, *cq, *buffer, color[5];
	FILE * f;
	int numcol, curcol, depth, linenum;
	int col, row, len, r, g, b;
	long rgb;
	
	f = fopen( filename, "r" );
	if ( !f ) {
		perror( filename );
		return 0;
	}
	numcol = -1;
	curcol = 0;
	linenum = 0;
	resetColor();
	row = by;
	while (fgets( line, sizeof line, f ) ) {
		linenum++;
		if ( line[0] != '"' )
			continue;
		if ( numcol == -1 ) {
			if ( sscanf( line+1, "%d%d%d%d", rw, rh, &numcol, &depth ) != 4 ) {
				fprintf( stderr, "bogus XPM header: %s:%d\n", filename, linenum );
				return 0;
			}
			if (!bm)
				return 1;
		} else if ( curcol < numcol ) {
			if ( strncmp( line+2, "\tc #", 4 ) != 0 ) {
				fprintf( stderr, "bogus XPM color line: %s:%d\n", filename, linenum );
				return 0;
			}
			color[3] = 0;
			memcpy( color, line+6, 2 );
			r = strtol( color, &cp, 16 );
			memcpy( color, line+10, 2 );
			g = strtol( color, &cp, 16 );
			memcpy( color, line+14, 2 );
			b = strtol( color, &cp, 16 );
			rgb = RGB(r,g,b);
			if ( curcol == 0 )
				fileColorMap[line[1]] = '0';
			else
				mapColor( line[1], rgb );
			curcol++;
		} else {
			if ( row > by+*rh ) {
				fprintf( stderr, "too many data lines: %s:%d\n", filename, linenum );
				return 0;
			}
			if ( row > bm->h )
				return 1;
			cp = line+1;
			for ( col=0; col<*rw; col++,cp++ ) {
				if ( bx+col > bm->w )
					break;
				if ( *cp == '"' ) {
					fprintf( stderr, "short data line: %s:%d\n", filename, linenum );
					return 0;
				}
				bm->b[bx+col+bm->w*row] = remapColor( *cp );
			}
			row++;
		}
	}
	fclose( f );
	return 1;
}

static void drawVline(
	bitmap_t * bm,
	int col,
	int x,
	int y0,
	int y1 )
{
	int jj;
	if ( x > bm->w )
		return;
	for ( jj=y0; jj<=y1; jj++ ) {
		if ( jj>=bm->h )
			return;
		bm->b[jj*bm->w+x] = col;
	}
}


static void drawHline(
	bitmap_t * bm,
	int col,
	int x0,
	int x1,
	int y )
{
	int ii;
	if ( y > bm->h )
		return;
	for ( ii=x0; ii<=x1; ii++ ) {
		if ( ii>=bm->w )
			return;
		bm->b[y*bm->w+ii] = col;
	}
}

static void fillBlock(
	bitmap_t * bm,
	int col,
	int x,
	int y,
	int w,
	int h )
{
	int ii, jj;
	for ( jj=y; jj<y+h; jj++ ) {
		if ( jj>bm->h )
			return;
		for ( ii=x; ii<x+w; ii++ ) {
			if ( ii>bm->w )
				return;
			bm->b[jj*bm->w+ii] = col;
		}
	}
}


int main ( int argc, char * argv[] )
{
	char * name;
	int colWhite, colMdGray, colDkGray, colBlack;
	int bx, w, h;
	bitmap_t bm;
	char ** filename;
	int ii, jj;
	char * cp;
	int argn;
	
	if ( argc < 3 ) {
		fprintf( stderr, "usage: %s NAME FILE1.XPM...\n", argv[0] );
		exit(1);
	}

	colMdGray = allocColor( RGB(0xC0,0xC0,0xC0) );
	colWhite = allocColor( RGB(255,255,255) );
	colDkGray = allocColor( RGB(0x80,0x80,0x80) );
	colBlack = allocColor( RGB(0,0,0) );
	allocColor( RGB(255,0,0) );

	name = argv[1];
	argc -= 2;
	filename = &argv[2];

	bm.w = 1;
	bm.h = 0;
	for ( argn=0; argn<argc; argn++ ) {
		if ( !read_xpm( filename[argn], &w, &h, bx+5, 5, NULL ) )
			return;
		if ( h+10 > bm.h )
			bm.h = h+10;
		bm.w += w+9;
	}

	bm.b = (char*)malloc( bm.w*bm.h );
	memset( bm.b, 0, bm.w*bm.h );
	fillBlock( &bm, colMdGray, 0, 0, bm.w, bm.h );
 	bx = 0;
	for ( argn=0; argn<argc; argn++ ) {
		if ( !read_xpm( filename[argn], &w, &h, bx+5, 5, &bm ) )
			return;
		drawVline( &bm, colBlack, bx+0, 0, bm.h-1 );
		drawVline( &bm, colWhite, bx+1, 1, bm.h-2 );
		drawVline( &bm, colWhite, bx+2, 1, bm.h-3 );
		drawVline( &bm, colDkGray, bx+5+w+2, 2, bm.h-2 );
		drawVline( &bm, colDkGray, bx+5+w+3, 1, bm.h-2 );
		drawVline( &bm, colBlack, bx+5+w+4, 0, bm.h-1 );

		drawHline( &bm, colBlack, bx+1, bx+5+w+3, 0 );
		drawHline( &bm, colWhite, bx+1, bx+5+w+2, 1 );
		drawHline( &bm, colWhite, bx+1, bx+5+w+1, 2 );
		drawHline( &bm, colDkGray, bx+2, bx+5+w+1, bm.h-3 );
		drawHline( &bm, colDkGray, bx+1, bx+5+w+1, bm.h-2 );
		drawHline( &bm, colBlack, bx+1, bx+5+w+3, bm.h-1 );

		bx += w+9;
	}

	fprintf( stdout, "/* XPM */\n" );
	fprintf( stdout, "static char * %s_xpm[] = {\n", name );
	fprintf( stdout, "\"%d %d %d 1\",\n", bm.w, bm.h, sizeColorMap()+1 );
	fprintf( stdout, "\"Z\tc #000000000000\",\n" );
	printColorMap( stdout );
	cp = bm.b;
	for ( jj=0; jj<bm.h; jj++ ) {
		fprintf(stdout, "\"%.*s\"%s\n", bm.w, cp, (jj<bm.h-1?",":"};") );
		cp += bm.w;
	}
}
