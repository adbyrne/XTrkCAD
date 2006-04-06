
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
#include <string.h>
#include <math.h>
#include <stdlib.h>
#if defined (__sun) && defined (__SVR4)
#include <ctype.h>
#endif
#include "readpng.h"

#define PROGNAME	"prochelp"

char line[1024];
int lineNum;
FILE * ifile;
FILE * ofile;
int wordwrap = 1;
int listLevel = -1;
int listType[10];
int listCount[10];
int lineWidth = 80;
int listWidth = 80;
int verbose = 0;
int toc = 0;
char * dirs[10] = { "." };
char ** dirList = &dirs[1];
int FontSize = 22;
double MarginTop = -1;
double MarginBottom = -1;
double MarginLeft = -1;
double MarginRight = -1;
double MarginGutter = -1;

#define LISTNONE	(0)
#define LISTBULLET	(1)
#define LISTDASH	(2)
#define LISTNUMBER	(3)

int USE_BMP = 0;


typedef struct {
	void (*start)( char *, char * );
	void (*finish)( void );
	void (*newParagraph)( void );
	void (*startLine)( int );
	void (*doBold)( char * );
	void (*doItalic)( char * );
	void (*doXref)( char *, char *, char * );
	void (*doPicture)( char *, int );
	void (*endLine)( void );
	void (*putChar)( char );
	void (*doSection)( char, char *, char *, char *, char *, int );
	void (*doHeader)( char * );
	void (*doStartDisplay)( void );
	void (*doEndDisplay)( void );
	void (*doThread)( char * );
	void (*doListStart)( void );
	void (*doListItem)( void );
	void (*doListEnd)( void );
	void (*page)( void );

	} dispatchTable;
dispatchTable *curMode;

struct tocList_t;
typedef struct tocList_t * tocList_p;
typedef struct tocList_t {
	tocList_p next;
	char section;
	long num;
	char * title;
	} tocList_t;
tocList_p tocHead = NULL;
tocList_p tocTail = NULL;
long tocNum = 37061946;
	

void no_op( void )
{
}
FILE * openFile( char * filename )
{
	FILE * f;
	char tmp[1024];
	char ** d;

	for ( d=dirs; *d; d++ ) {
		sprintf( tmp, "%s/%s", *d, filename );
		f = fopen( tmp, "r" );
		if (f)
			return f;
	}
	fprintf( stderr, "Can't open %s\n", filename );
	exit(1);
}

void normalStart( char * inName, char * outName )
{
	ifile = openFile( inName );
	if ( strcmp( outName, "-" ) == 0 ) {
		ofile = stdout;
	} else {
		ofile = fopen( outName, "w" );
		if (ofile == NULL) {
			perror( outName );
			exit( 1 );
		}
	}
}
void normalFinish( void )
{
	if (ofile)
		fclose( ofile );
}
void process( FILE * );

/******************************************************************************
 *
 *	COMMON RTF
 *
 *****************************************************************************/

int rtfNeedPar = FALSE;
int rtfNeedGap = FALSE;
int rtfNeedFI0 = FALSE;
int rtfGapHeight = -1;
int rtfBigGap = 60;

void rtfFlushParagraph( void )
{
	if ( rtfNeedPar ) {
		if ( rtfNeedGap==TRUE && rtfGapHeight!=rtfBigGap ) {
			fprintf( ofile, "\\sb%d", rtfBigGap );
			rtfGapHeight = rtfBigGap;
		}
		if ( rtfNeedGap==FALSE && rtfGapHeight!=0 ) {
			fprintf( ofile, "\\sb0" );
			rtfGapHeight = 0;
		}
		fprintf( ofile, "\\par\n" );
		if ( rtfNeedFI0 )
			fprintf( ofile, "\\fi0\n" );
		rtfNeedPar = FALSE;
		rtfNeedGap = FALSE;
		rtfNeedFI0 = FALSE;
	}
}


void rtfPutChar( char ch )
{
	if ( ((ch) & 0x80) ){
		fprintf( ofile, "\\'%2.2X", (unsigned char)ch );
	} else if ( (ch) == '\\' ){
		fprintf( ofile, "\\\\" );
	} else {
		fputc( ch, ofile );
	}
	rtfNeedPar = TRUE;
}
void rtfPutString( char * cp )
{
	while (*cp) {
		rtfPutChar( *cp++ );
	}
}
void rtfNewParagraph( void )
{
	if ( wordwrap ) {
		rtfFlushParagraph();
#ifdef LATER
		if ( listLevel < 0 ) {
			rtfFlushParagraph();
			rtfNeedGap = 1;
		} else {
			if ( rtfNeedPar ) {
				fprintf( ofile, "\\line\r\n" );
				rtfNeedPar = FALSE;
			}
		}
#endif
	}
}
void rtfStartLine( int lastBlank )
{
	if ( !wordwrap ) {
		fprintf( ofile, "\\tab\r\n" );
	} else if ( lastBlank ) {
		rtfFlushParagraph();
	}
}
void rtfBold( char * name )
{
	fprintf( ofile, "{\\b " );
	rtfPutString( name );
	fprintf( ofile, "}" );
}
void rtfItalic( char * name )
{
	fprintf( ofile, "{\\i " );
	rtfPutString( name );
	fprintf( ofile, "}" );
}
void rtfEndLine( void )
{
	if ( !wordwrap ) {
		rtfNeedPar = TRUE;
		rtfFlushParagraph();
	}
}
void rtfStartDisplay( void )
{
	rtfFlushParagraph();
}
void rtfListStart( void )
{
	rtfFlushParagraph();
	if (listLevel>0) {
		fprintf( ofile, "\\pard" );
/*
		if ( rtfNeedGap ) {
			fprintf( ofile, "\\sb%d", rtfBigGap );
			rtfGapHeight = rtfBigGap;
			rtfNeedGap = FALSE;
		}
*/
		rtfGapHeight = -1;
	}
	fprintf( ofile, "\\tx360\\li%d\r\n", 360*(listLevel+1) );
}
void rtfListItem( void )
{
	/*if (listLevel == 0 || listCount[listLevel] > 1)*/
	rtfFlushParagraph();
	fprintf( ofile, "\\fi-360 " );
	rtfNeedFI0 = TRUE;
	switch (listType[listLevel]) {
	case LISTNONE:
#ifdef LATER
		if ( listCount[listLevel] > 0 )
			fprintf( ofile, "\\fi-360 " );
		rtfNeedFI0 = TRUE;
#endif
		break;
	case LISTBULLET:
		fprintf( ofile, "{\\f1\\'B7}\\tab" );
		break;
	case LISTDASH:
		fprintf( ofile, "{\\b -}\\tab" );
		break;
	case LISTNUMBER:
		fprintf( ofile, "{\\b %d}\\tab", listCount[listLevel] );
		break;
	}
	fprintf( ofile, "\r\n" );
}
void rtfListEnd( void )
{
	if (listLevel == -1)
		fprintf( ofile, "\\par\\pard\r\n" );
	else
		fprintf( ofile, "\\par\\pard\\tx360\\li%d\\fi-360\r\n", 360*(listLevel+1) );
	rtfNeedPar = FALSE;
	rtfGapHeight = -1;
	rtfNeedGap = FALSE;
}

void rtfPage( void )
{
	rtfFlushParagraph();
	fprintf( ofile, "\\page\r\n" );
}

/******************************************************************************
 *
 *	MSW-HELP
 *
 *****************************************************************************/

int pageCnt = 0;

struct {
	char * name;
	int count;
	} threads[100];
int threadCnt = 0;


char * remap_minus( char * cp )
{
    char * cp0 = cp;
    for ( ; *cp; cp++ )
	if ( *cp == '-' )
	    *cp = '_';
    return cp0;
}

int lookupThread( char * name )
{
	int inx;
	if (!name) {
		fprintf( stderr, "%d: NULL thread string\n", lineNum );
		return 0;
	}
	for (inx=0;inx<threadCnt;inx++) {
		if (strcmp(threads[inx].name,name)==0) {
			return ++(threads[inx].count);
		}
	}
	threads[threadCnt].name = strdup( name );
	threads[threadCnt].count = 1;
	threadCnt++;
	return 1;
}

void mswhelpXref( char * name, char * ref1, char * ref2 )
{
	fprintf( ofile, "{\\uldb " );
	rtfPutString( name );
	fprintf( ofile, "}{\\v %s}", ref1 );
}
void mswhelpPicture( char * name, int inLine )
{
	if (inLine) {
		fprintf( ofile, "\\{bml %s.shg\\}\\tab", name );
		rtfNeedPar = TRUE;
	} else {
		fprintf( ofile, "{\\qc\\{bmc %s.shg\\}\\par\\pard}\r\n", name );
		rtfNeedPar = FALSE;
		rtfNeedGap = FALSE;
		rtfGapHeight = -1;
	}
}
void mswhelpSection( char section, char * title, char * context, char * picture, char * keywords, int newpage )
{
	if (pageCnt != 0 && newpage)
		fprintf( ofile, "\\page\r\n" );
	pageCnt++;
	if (context && context[0] != '\0')
		fprintf( ofile, "#{\\footnote %s}\r\n", remap_minus(context) );
	if (newpage && title && title[0] != '\0') {
		fprintf( ofile, "${\\footnote %s}\r\n", title );
	}
	if (keywords && keywords[0] != '\0')
		fprintf( ofile, "K{\\footnote %s}\r\n", keywords );
	if (picture && picture[0] != '\0') {
		mswhelpPicture( picture, 1 );
	}
	if (title && title[0] != '\0')
		fprintf( ofile, "{\\b\\fs30 %s}\r\n", title );
	fprintf( ofile, "\\par\r\n" );
	rtfNeedPar = FALSE;
}
void mswhelpHeader( char * line )
{
	if ( line[0] == '*' )
		return;
	fprintf( ofile, "#{\\footnote %s}\r\n", remap_minus(line) );
}
void mswhelpThread( char * thread )
{
	int threadCnt;
	threadCnt = lookupThread( thread );
	fprintf( ofile, " +{\\footnote %s:%02d}\r\n", thread, threadCnt );
}
dispatchTable mswhelpTable = {
	normalStart,
	normalFinish,
	rtfNewParagraph,
	rtfStartLine,
	rtfBold,
	rtfItalic,
	mswhelpXref,
	mswhelpPicture,
	rtfEndLine,
	rtfPutChar,
	mswhelpSection,
	mswhelpHeader,
	rtfStartDisplay,
	(void*)no_op,
	mswhelpThread,
	rtfListStart,
	rtfListItem,
	rtfListEnd,
	(void*)no_op
	};

/******************************************************************************
 *
 *	MSW-WORD
 *
 *****************************************************************************/


struct BMFH {
	char type[2];
	short size[2];
	short rsvd1;
	short rsvd2;
	short off[2];
	};

struct BMIH {
	long size;
	long width;
	long height;
	short planes;
	short colors;
	long comp;
	long imageSize; 
	long xPelsPerMeter;
	long yPelsPerMeter;
	long clrUsed;
	long clrImportant; 
	};

struct BMIH2 {
	long size;
	short width;
	short height;
	short planes;
	short colors;
	};

unsigned short S( short val )
{
    union {
	short inVal;
	unsigned char outVal[2];
	} convShort;
    short ret;
    convShort.inVal = val;
    ret = (((short)convShort.outVal[0])<<8) + ((short)convShort.outVal[1]);
    return ret;
}


long L( long val )
{
    union {
	long inVal;
	unsigned char outVal[4];
	} convLong;
    long ret;
    convLong.inVal = val;
    ret = (((long)convLong.outVal[0])<<24) + (((long)convLong.outVal[1])<<16) + (((long)convLong.outVal[2])<<8) + ((long)convLong.outVal[3]);
    return ret;
}


void dumpBytes( char * buff, long size, FILE * outF )
{
    long inx, off, rc;
    for (inx=0, off=1; inx<size; inx++,off++) {
	fprintf( outF, "%0.2x", (unsigned char)buff[inx] );
	if (off >= 40) {
		fprintf( outF, "\n" );
		off = 0;
	}
    }
    if (off != 1)
	fprintf( outF, "\n" );
}


void conv24to8( long * colorTab, unsigned char * buff, int channels, int width24, int width8, int height )
{
	long * lastColor, *cp;
	long color;
	unsigned char * ip;
	unsigned char *op;
	int h, w;
	lastColor = colorTab;
	memset( colorTab, 0, 1024 );
	op = buff;
	for (h=0; h<height; h++) {
		ip = buff+(width24*h);
		op = buff+(width8*h);
		for (w=0; w<width24; w+=channels,op++ ) {
			color =  ((long)(ip[0]))<<16;
			color += ((long)(ip[1]))<<8;
			color += ((long)(ip[2]));
			ip += channels;
			for ( cp=colorTab; cp<lastColor; cp++ ) {
				if (color == *cp) {
					*op = (unsigned char)(cp-colorTab);
					goto nextPixel;
				}
			}
			if (lastColor < &colorTab[256]) {
				*op = (unsigned char)(lastColor-colorTab);
				*lastColor++ = color;
			} else {
				*op = 0;
			}
nextPixel:
			;
		}
		*op++ = 0;
		*op++ = 0;
	}
} 


void dumpBmp( char * bmpName, FILE * outF )
{
	long rc;
	long size;
	long fullSize;
	long scanWidth, width8;
	long h;
	long picw, pich;
	long fileSize, maxRecSize;
	struct BMFH bmfh;
	struct BMIH bmih;
	struct BMIH2 bmih2;
	char * buff;
	long * colorTab;
	long bmfhSize, bmfhOff;
	FILE * bmpF;
	int colormapOff;
	int i, j;

	bmpF = openFile( bmpName );
	rc = fread( &bmfh, 1, sizeof bmfh, bmpF );
	rc = fread( &bmih, 1, sizeof bmih, bmpF );
	colormapOff = sizeof bmfh + sizeof bmih;
	if (bmih.size == 12L) {
	    fseek( bmpF, sizeof bmfh, SEEK_SET );
	    rc = fread( &bmih2, 1, sizeof bmih2, bmpF );
	    bmih.width = bmih2.width;
	    bmih.height = bmih2.height;
	    bmih.planes = bmih2.planes;
	    bmih.colors = bmih2.colors;
	    bmih.comp = 0;
	    bmih.imageSize = 0;
	    bmih.xPelsPerMeter = 0;
	    bmih.yPelsPerMeter = 0;
	    bmih.clrUsed = 0;
	    bmih.clrImportant = 0;
	    colormapOff = sizeof bmfh + sizeof bmih2;
	}
#ifdef LATER
	bmfh.size = L(bmfh.size);
	bmfh.off = L(bmfh.off);
	bmih.size = L(bmih.size);
	bmih.width = L(bmih.width);
	bmih.height = L(bmih.height);
	bmih.planes = S(bmih.planes);
	bmih.colors = S(bmih.colors);
	bmih.comp = L(bmih.comp);
	bmih.imageSize = L(bmih.imageSize);
	bmih.xPelsPerMeter = L(bmih.xPelsPerMeter);
	bmih.yPelsPerMeter = L(bmih.yPelsPerMeter);
	bmih.clrUsed = L(bmih.clrUsed);
	bmih.clrImportant = L(bmih.clrImportant);
#endif
	bmfhSize = ((unsigned short)bmfh.size[0]) + (((long)bmfh.size[1])<<16);
	bmfhOff = ((unsigned short)bmfh.off[0]) + (((long)bmfh.off[1])<<16);
	if (verbose) {
	fprintf( stdout, "BMFH  type %c%c,  size %ld,  off %ld\n",
		bmfh.type[0], bmfh.type[1], bmfhSize, bmfhOff );
	fprintf( stdout, "BMIH  size %ld,  width %ld,  height %ld,  planes %d,  colors %d\n  comp %ld,  imageSize %ld,  xDPM %ld,  yDPM %ld,  clrUsed %ld,  clrImportant %ld\n",
		bmih.size, bmih.width, bmih.height, bmih.planes, bmih.colors,
		bmih.comp, bmih.imageSize, bmih.xPelsPerMeter, bmih.yPelsPerMeter,
		bmih.clrUsed, bmih.clrImportant );
	}
	scanWidth = (bmih.width*bmih.colors+7)/8;
	scanWidth = ((scanWidth+3)/4)*4;
	fullSize = size = bmfhSize - bmfhOff;
	if ( fullSize != bmih.height*scanWidth ) {
		fprintf( stderr, "%s: height*scanWidth(%ld)(%ld) != fullSize(%ld)\n", bmpName, scanWidth, bmih.height*scanWidth, fullSize );
		return;
	}
	if ( bmih.colors != 24 && bmih.colors != 8 && bmih.colors != 4 && bmih.colors != 1) {
		return;
	}
	if ( bmih.planes != 1 ) {
		fprintf( stderr, "%s: planes(%d) != 1\n", bmpName, bmih.planes );
		return;
	}
	if ( bmih.comp != 0 ) {
		fprintf( stderr, "%s: comp(%d) != 0\n", bmpName, bmih.comp );
		return;
	}

	if (bmih.colors != 8) {
		size = (((bmih.width+3)/4)*4) * bmih.height;
	}
	fileSize = (size+1024)/2 + 70;
	maxRecSize = (size+1024)/2 + 34;

	picw = bmih.width*26L;
	pich = bmih.height*26L;
	if ( outF ) {
	buff = NULL;
	fprintf( outF, "{\\pict\\wmetafile8\\picw%ld\\pich%ld\\picwgoal%ld\\pichgoal%ld\\picbmp\\picbpp%d\n",
		picw, pich, bmih.width*15L, bmih.height*15L, 8/*bmih.colors*/ );
	fprintf( outF, "010009000003%0.8lx0000%0.8lx0000\n",
		L(fileSize), L(maxRecSize) );
	fprintf( outF, "050000000b0200000000\n" ); /* SetWindowOrg(0,0) */
	fprintf( outF, "050000000c02%0.4x%0.4x\n", S((short)bmih.height), S((short)bmih.width) );
	fprintf( outF, "05000000090200000000\n" ); /* SetTextColor( 0 ) */
	fprintf( outF, "050000000102d8d0c800\n" ); /* SetBkColor( 0 ) */
	fprintf( outF, "0400000007010300\n" ); /* SetStretchBltMode(0300) */
	fprintf( outF, "%0.8lx430f\n", L(maxRecSize) );
	fprintf( outF, "2000cc000000%0.4x%0.4x00000000%0.4x%0.4x00000000\n",
		S((short)bmih.height), S((short)bmih.width), S((short)bmih.height), S((short)bmih.width) );
	fprintf( outF, "28000000%0.8lx%0.8lx%0.4x%0.4x000000000000000000000000000000000000000000000000\n",
		L(bmih.width), L(bmih.height), S(bmih.planes), S(8/*bmih.colors*/) );
	switch ( bmih.colors ) {
	case 8:
		buff = (char*)malloc(1024);
		fseek( bmpF, colormapOff, 0 );
		rc = fread( buff, 1024, 1, bmpF );
		if (bmih.size == 12L) {
		    for (h=255; h>=0; h--) {
		        for (i=3; i>=0; i--)
			    buff[h*4+i] = buff[h*3+i];
		        buff[h*4+3] = 0;
		    }
		}
		dumpBytes( buff, 1024, outF );
		rc = fseek( bmpF, bmfhOff, 0 );
		buff = (char*)realloc( buff, (int)scanWidth );
		for ( h=0; h<bmih.height; h++ ) {
			rc = fread( buff, (int)scanWidth, 1, bmpF );
			dumpBytes( buff, scanWidth, outF );
		}
		break;
	case 4:
		buff = (char*)malloc(1024);
		fseek( bmpF, colormapOff, 0 );
		memset( buff, 0, 1024 );
		rc = fread( buff, 3*16, 1, bmpF );
		for (h=15; h>=0; h--) {
		    for (i=3; i>=0; i--)
			buff[h*4+i] = buff[h*3+i];
		    buff[h*4+3] = 0;
		}
		dumpBytes( buff, 1024, outF );
		rc = fseek( bmpF, bmfhOff, 0 );
		buff = (char*)realloc( buff, (int)scanWidth*2+10 );
		width8 = (bmih.width+3)/4*4;
		for ( h=0; h<bmih.height; h++ ) {
			rc = fread( buff, (int)scanWidth, 1, bmpF );
			for (i=scanWidth-1; i>=0; i--) {
			    buff[i*2+1] = buff[i]&0xF;
			    buff[i*2] = (buff[i]>>4)&0xF;
			}
			dumpBytes( buff, width8, outF );
		}
		break;
	case 1:
		buff = (char*)malloc(1024);
		fseek( bmpF, colormapOff, 0 );
		memset( buff, 0, 1024 );
		rc = fread( buff, 3*2, 1, bmpF );
		for (h=1; h>=0; h--) {
		    for (i=3; i>=0; i--)
			buff[h*4+i] = buff[h*3+i];
		    buff[h*4+3] = 0;
		}
		dumpBytes( buff, 1024, outF );
		rc = fseek( bmpF, bmfhOff, 0 );
		buff = (char*)realloc( buff, (int)scanWidth*8+10 );
		width8 = (bmih.width+3)/4*4;
		for ( h=0; h<bmih.height; h++ ) {
			rc = fread( buff, (int)scanWidth, 1, bmpF );
			for (i=scanWidth-1; i>=0; i--) {
			    for (j=7; j>=0; j--) {
				buff[i*8+j] = (buff[i]&(128>>j))?1:0;
			    }
			}
			dumpBytes( buff, width8, outF );
		}
		break;
	case 24:
		buff = (char*)malloc( (int)(fullSize) );
		rc = fread( buff, (int)(fullSize), 1, bmpF );
		colorTab = (long*)malloc( 1024 );
		width8 = ((bmih.width+3)/4)*4;
		conv24to8( colorTab, buff, (int)scanWidth, 3, (int)width8, (int)bmih.height );
		dumpBytes( (char*)colorTab, 1024, outF );
		for ( h=0; h<bmih.height; h++ ) {
			dumpBytes( buff, (int)width8, outF );
			buff += (int)width8;
		}
		break;
	default:
		fprintf( stderr, "%s: colors(%d) != 24|8|4|1\n", bmpName, bmih.colors );
		return;
	}
	fprintf( outF, "030000000000\n" );
	fprintf( outF, "}\n" );
	}
	fclose( bmpF );
}


void dumpPng(
	char * fileName,
	FILE * outF )
{
	FILE * pngF;
	int rc;
	unsigned long image_rowbytes, width8, h;
	long image_width, image_height;
	int image_channels;
	unsigned char * image_data;
	double display_exponent = 1.0;

	int bmih_colors = 24;
	int bmih_planes = 1;

	long size, fileSize, maxRecSize;
	long colorTab[1024];
 	char pathName[1024];
	char ** dir;

	for ( dir=dirs; *dir; dir++ ) {
		sprintf( pathName, "%s/%s", *dir, fileName );
		pngF = fopen( pathName, "r" );
		if ( pngF != NULL )
			break;
	}

	if ( pngF == NULL ) {
		perror( fileName );
		return;
	}
        if ((rc = readpng_init(pngF, (long *) &image_width, (long *) &image_height)) != 0) {
            switch (rc) {
                case 1:
                    fprintf(stderr, PROGNAME
                      ":  [%s] is not a PNG file: incorrect signature\n",
                      pathName);
                    break;
                case 2:
                    fprintf(stderr, PROGNAME
                      ":  [%s] has bad IHDR (libpng longjmp)\n",
                      pathName);
                    break;
                case 4:
                    fprintf(stderr, PROGNAME ":  insufficient memory\n");
                    break;
                default:
                    fprintf(stderr, PROGNAME
                      ":  unknown readpng_init() error\n");
                    break;
            }
            return;
	}

	image_data = readpng_get_image(display_exponent, &image_channels, &image_rowbytes);
	width8 = ((image_width+3)/4)*4;
	size = width8*image_height;
	fileSize = (size+1024)/2 + 70;
        maxRecSize = (size+1024)/2 + 34;

	fprintf( outF, "{\\pict\\wmetafile8\\picw%ld\\pich%ld\\picwgoal%ld\\pichgoal%ld\\picbmp\\picbpp%d\n",
		image_width*26L, image_height*26L, image_width*15L, image_height*15L, 8/*bmih_colors*/ );
	fprintf( outF, "010009000003%0.8lx0000%0.8lx0000\n",
		L(fileSize), L(maxRecSize) );
	fprintf( outF, "050000000b0200000000\n" ); /* SetWindowOrg(0,0) */
	fprintf( outF, "050000000c02%0.4x%0.4x\n", S((short)image_height), S((short)image_width) );
	fprintf( outF, "05000000090200000000\n" ); /* SetTextColor( 0 ) */
	fprintf( outF, "050000000102d8d0c800\n" ); /* SetBkColor( 0 ) */
	fprintf( outF, "0400000007010300\n" ); /* SetStretchBltMode(0300) */
	fprintf( outF, "%0.8lx430f\n", L(maxRecSize) );
	fprintf( outF, "2000cc000000%0.4x%0.4x00000000%0.4x%0.4x00000000\n",
		S((short)image_height), S((short)image_width), S((short)image_height), S((short)image_width) );
	fprintf( outF, "28000000%0.8lx%0.8lx%0.4x%0.4x000000000000000000000000000000000000000000000000\n",
		L(image_width), L(image_height), S(bmih_planes), S(8/*bmih.colors*/) );
	width8 = ((image_width+3)/4)*4;
	conv24to8( colorTab, (char *) image_data, image_channels, image_width*image_channels, width8, image_height );
	dumpBytes( (char *)colorTab, 1024, outF );
	for ( h=0; h<image_height; h++ ) {
		dumpBytes( (char *) image_data+(h)*width8, (int)width8, outF );
	}
	fprintf( outF, "030000000000\n" );
	fprintf( outF, "}\n" );

	readpng_cleanup(0);
	fclose( pngF );
	free( image_data );

}

void mswwordXref( char * name, char * ref1, char * ref2 )
{
	rtfBold( name );
}
void mswwordPicture( char * name, int inLine )
{
	char tmp[80];
	if (!inLine) {
		rtfFlushParagraph();
		fprintf( ofile, "{\\qc\\sb%d", rtfNeedGap?rtfBigGap:0 );
	}
	if ( USE_BMP ) {
		sprintf( tmp, "%s.bmp", name );
		dumpBmp( tmp, ofile );
	} else {
		sprintf( tmp, "%s.png", name );
		dumpPng( tmp, ofile );
	}
	if (inLine) {
		/*fprintf( ofile, "\\tab " );*/
		rtfNeedPar = TRUE;
	} else {
		fprintf( ofile, "\\par\\pard}\n" );
		rtfNeedPar = FALSE;
		rtfNeedGap = FALSE;
		rtfGapHeight = -1;
	}
}
int sectionNum[3] = { 0, 0, 0 };
void mswwordSection( char section, char * title, char * context, char * picture, char * keywords, int newpage )
{
	char tmp[1024];
	char sectionNumS[20];
	rtfFlushParagraph();
	if (pageCnt != 0 && newpage) {
		fprintf( ofile, "\\page\n" );
	} 
	pageCnt++;
	if (toc) {
		fprintf( ofile, "{\\*\\bkmkstart _Toc%ld}\n", tocNum );
	}
	fprintf( ofile,"\
{\\pntext\\pard\\plain\\b\\f5\\fs28\\kerning28 \
" );
	switch ( section ) {
	case 'A':
		sprintf( sectionNumS, "%d. ", ++sectionNum[0] );
		sectionNum[1] = sectionNum[2] = 0;
		break;
	case 'B':
		sprintf( sectionNumS, "%d.%d ", sectionNum[0], ++sectionNum[1] );
		sectionNum[2] = 0;
		break;
	case 'C':
#ifdef LATER
		sprintf( sectionNumS, "%d.%d.%d ", sectionNum[0], sectionNum[1], ++sectionNum[2] );
#else
		sprintf( sectionNumS, "" );
#endif
		break;
	default:
		sprintf( sectionNumS, "bad section (%c) ", section );
	}
	fprintf( ofile, "\
%s\\tab}\
\\pard\\plain \\s%d\\sb240\\sa60\\keepn\\widctlpar\
{\\*\\pn \\pnlvl%d\\pndec\\pnprev1\\pnstart1\\pnindent720\\pnhang\
{\\pntxta .}\
}\
\\b\\f5\\fs28\\kerning28 %s\
", sectionNumS, section-'A'+1, section-'A'+1, title );
	if (picture && picture[0] != '\0') {
		fprintf( ofile, " " );
		mswwordPicture( picture, 1 );
	}
	if (toc) {
		fprintf( ofile, "{\\*\\bkmkend _Toc%ld}\n", tocNum );
	}
	fprintf( ofile, "\
\\par \
\\pard\\plain \\widctlpar \\f4\\fs%d \
\n", FontSize );
	if (toc) {
		tocList_p tl;
		tl = (tocList_p)malloc( sizeof *tl );
		tl->section = section;
		tl->title = (char*)malloc( strlen(sectionNumS) + strlen(title) + 1 );
		sprintf( tl->title, "%s%s", sectionNumS, title );
		tl->num = tocNum++;
		tl->next = NULL;
		if (tocHead == NULL)
			tocHead = tl;
		else
			tocTail->next = tl;
		tocTail = tl;
	}
	rtfNeedPar = FALSE;
	rtfNeedGap = TRUE;
	rtfGapHeight = -1;
}

void mswwordStart( char * inName, char * outName )
{
	normalStart( inName, outName );
	if ( MarginGutter >= 0.0 )
		fprintf( ofile, "\\margmirror\\gutter%d\n", (int)(MarginGutter*1440.0) );
	if (MarginTop >= 0.0)
		fprintf( ofile, "\\margt%d\n", (int)(MarginTop*1440.0) );
	if (MarginBottom >= 0.0)
		fprintf( ofile, "\\margb%d\n", (int)(MarginBottom*1440.0) );
	if (MarginRight >= 0.0)
		fprintf( ofile, "\\margr%d\n", (int)(MarginRight*1440.0) );
	if (MarginLeft >= 0.0)
		fprintf( ofile, "\\margl%d\n", (int)(MarginLeft*1440.0) );
}

void mswwordFinish( void )
{
	char lastSection = 'A';
	tocList_p tl;
	rtfFlushParagraph();
	if (toc) {
		fprintf( ofile, "\
\\sect \\sectd \\pgnrestart\\pgnlcrm\\linex0\\endnhere\
\\pard\\plain \\qc\\widctlpar \\f4\\fs22 \
{\\b\\fs36\\lang1024\\kerning28 Contents \\par \\par }\
\\pard\\plain \\s17\\widctlpar\\tqr\\tldot\\tx8640 \\f4\\fs%d\n", FontSize );
		for ( tl=tocHead; tl; tl=tl->next ) {
			if ( tl->section != lastSection ) {
				fprintf( ofile, "\
\\pard\\plain \\s%d\\li%d\\widctlpar\\tqr\\tldot\\tx8640 \\f4\\fs%d\n",
					tl->section-'A'+17,
					(tl->section-'A')*200, FontSize );
				lastSection = tl->section;
			}
			fprintf( ofile, "\
{\\lang1024\\kerning28 %s}{\\lang1024 \\tab }\
{\\field{\\*\\fldinst {\\lang1024  GOTOBUTTON _Toc%ld  }\n\
{\\field{\\*\\fldinst {\\lang1024  PAGEREF _Toc%ld }}\
{\\fldrslt {\\lang1024 3}}}}}{\\lang1024 \\par }\n",
				tl->title, tl->num, tl->num);
		}
		fprintf( ofile,
"\\pard\\plain \\widctlpar \\f4\\fs%d\n}\n}\n"
/*\\pard\\plain \*/
"\\widctlpar \\f4\\fs%d\n" , FontSize, FontSize);
	}
	normalFinish();
}


dispatchTable mswwordTable = {
	mswwordStart,
	mswwordFinish,
	rtfNewParagraph,
	rtfStartLine,
	rtfBold,
	rtfItalic,
	mswwordXref,
	mswwordPicture,
	rtfEndLine,
	rtfPutChar,
	mswwordSection,
	(void*)no_op,
	rtfStartDisplay,
	(void*)no_op,
	(void*)no_op,
	rtfListStart,
	rtfListItem,
	rtfListEnd,
	rtfPage
	};

/******************************************************************************
 *
 *	TEXT
 *
 *****************************************************************************/

char textBuff[1024];
char *textBuffP = textBuff;
int textNewLine = 1;
int textIndent = 0;
int textAllowLeadingBlanks = 0;
int textNoIndent = 0;
int textLineLength;

void textPutChar( char ch )
{
	char *cp, *cq;
	int indent;
	int width;

	if (textNewLine) {
		textLineLength = 0;
		if (ch == ' ' && !textAllowLeadingBlanks) {
			return;
		}
		if (!textNoIndent) {
			for (indent=0; indent<textIndent; indent++) {
				memmove( textBuffP, "    ", 4 );
				textBuffP += 4;
				textLineLength += 4;
			}
		}
	}
	textNewLine = 0;
	*textBuffP++ = ch;
	if (ch == '\010')
		textLineLength--;
	else
		textLineLength++;
	width = (textIndent>0?listWidth:lineWidth);
	if ( wordwrap && width > 0 && textLineLength > width ) {
		for (cp = textBuffP-1; *cp != ' ' && cp>textBuff+lineWidth/2; cp-- );
		while ( *cp == ' ' && cp>textBuff+lineWidth/2 ) cp--;
		cp++;
		fwrite( textBuff, cp-textBuff, 1, ofile );
		fwrite( "\n", 1, 1, ofile );
		textNewLine = 1;
		while (*cp == ' ' && cp<textBuffP) cp++;
		if (textBuffP!=cp) {
			cq = textBuff+textIndent*4;
			memmove( cq, cp, textBuffP-cp );
			cq = textBuff;
			for (indent=0; indent<textIndent; indent++) {
				memmove( cq, "    ", 4 );
				cq += 4;
			}
			textBuffP = cq + (textBuffP-cp);
			textNewLine = 0;
			for ( cp=textBuff,textLineLength=0; cp<textBuffP; cp++ ) {
				if (*cp == '\010')
					textLineLength--;
				else
					textLineLength++;
			}
		} else {
			textBuffP = textBuff;
		}
	} else if (textBuffP - textBuff >= sizeof textBuff ) {
		fwrite( textBuff, textBuffP-textBuff, 1, ofile );
		textBuffP = textBuff;
		textLineLength = 0;
	}
}
void textBreakLine( void )
{
	if ( !textNewLine ) {
		fwrite( textBuff, textBuffP-textBuff, 1, ofile );
		fwrite( "\n", 1, 1, ofile );
		textNewLine = 1;
		textBuffP = textBuff;
		textLineLength = 0;
	}
}
void textSaveLine( char * tmp )
{
	if (!textNewLine) {
		int len = textBuffP-textBuff;
		memcpy( tmp, textBuff, len );
		tmp[len] = '\0';
		textNewLine = 1;
		textBuffP = textBuff;
		textLineLength = 0;
	} else {
		tmp[0] = '\0';
	}
}
void textRestoreLine( char * tmp )
{
	int len = strlen( tmp );
	if (len > 0) {
		memcpy( textBuffP, tmp, len );
		textBuffP += len;
		textLineLength += len;
		textNewLine = 0;
	}
}
void textFinish( void )
{
	textBreakLine();
	normalFinish();
}
void textPutString( char * cp )
{
	while (*cp)
		textPutChar( *cp++ );
}
void textNewParagraph( void )
{
	textBreakLine();
	if (wordwrap) {
		fwrite( "\n", 1, 1, ofile );
	}
}
void textStartLine( int lastlineblank )
{
}
void textBold( char * name )
{
	char * cp;
	/*textPutChar( '<' );*/
	for ( cp = name; *cp; cp++ ) {
		textPutChar( *cp );
		if (*cp != ' ') {
			textPutChar( '\010' );
			textPutChar( *cp );
		}
	}
	/*textPutString( name );*/
	/*textPutChar( '>' );*/
}
void textItalic( char * name )
{
	char * cp;
	/*textPutChar( '<' );*/
	for ( cp = name; *cp; cp++ ) {
		textPutChar( *cp );
		if (*cp != ' ') {
			textPutChar( '\010' );
			textPutChar( *cp );
		}
	}
	/*textPutString( name );*/
	/*textPutChar( '>' );*/
}
void textXref( char * name, char * ref1, char * ref2 )
{
	textBold( name );
	/*textPutChar( '<' );
	textPutString( name );
	textPutChar( '>' );*/
	if (ref2) {
		textPutString( " (See " );
		textPutString( ref2 );
		textPutString( " for Details)" );
	}
}
void textPicture( char * picture, int inLine )
{
	textPutString( "<<" );
	textPutString( picture );
	textPutString( ">>" );
	if (inLine) {
		textPutString( "  " );
	} else {
		textBreakLine();
		fwrite( "\n", 1, 1, ofile );
	}
}
void textEndLine( void )
{
	if ( !wordwrap )
		textBreakLine();
}
void textSection( char section, char * title, char * context, char * picture, char * keywords, int newpage )
{
	int len;
	textBreakLine();
	if (pageCnt > 0 && newpage) {
		fwrite( "\014\n", 1, 2, ofile );
	}
	pageCnt++;
	textBold( title );
	/*textPutString( title );*/
	textBreakLine();
	for ( len = strlen(title); len>0; len-- ) {
		textBold( "=" );
		/*fwrite( "=", 1, 1, ofile );*/
	}
	textBreakLine();
	fwrite( "\n", 1, 1, ofile );
}
void textHeader( char * line )
{
}
void textStartIndent( void )
{
	textBreakLine();
	textIndent++;
}
void textEndIndent( void )
{
	textBreakLine();
	if (textIndent < 0) {
		fprintf( stderr, "%d: textIndent < 0\n", lineNum );
		textIndent = 0;
	} else {
		textIndent--;
	}
}
void textListItem( void )
{
	char num[4];
	textBreakLine();
	textIndent--;
	textAllowLeadingBlanks = 1;
	switch( listType[listLevel] ) {
	case LISTNONE:
	default:
		textPutString( "  " );
		break;
	case LISTBULLET:
		textPutString( "  o " );
		break;
	case LISTDASH:
		textPutString( "  - " );
		break;
	case LISTNUMBER:
		sprintf( num, "%3.3d", listCount[listLevel] );
		textPutString( num );
		textPutChar( ' ' );
		break;
	}
	textAllowLeadingBlanks = 0;
	textIndent++;
}
void textPage( void )
{
	fwrite( "\014\n", 1, 2, ofile );
}
dispatchTable textTable = {
	normalStart,
	textFinish,
	textNewParagraph,
	textStartLine,
	textBold,
	textItalic,
	textXref,
	textPicture,
	textEndLine,
	textPutChar,
	textSection,
	textHeader,
	textStartIndent,
	textEndIndent,
	(void*)no_op,
	textStartIndent,
	textListItem,
	textEndIndent,
	textPage
	};

/******************************************************************************
 *
 *	XVIEW
 *
 *****************************************************************************/


void xviewStart( char * inName, char * outName )
{
	normalStart( inName, outName );
	lineWidth = 0;
}

void xviewBold( char * name )
{
	char * cp;
	textPutChar( '<' );
	textPutString( name );
	textPutChar( '>' );
}
void xviewItalic( char * name )
{
	char * cp;
	textPutChar( '<' );
	textPutString( name );
	textPutChar( '>' );
}
void xviewXref( char * name, char * ref1, char * ref2 )
{
	xviewBold( name );
	if (ref2) {
		textPutString( " (See " );
		textPutString( ref2 );
		textPutString( " for Details)" );
	}
}
void xviewSection( char section, char * title, char * context, char * picture, char * keywords, int newpage )
{
	int indent;
	int len;

	static char * stars = "************";
	indent = line[1]-'A'+1;
	textBreakLine();
	if (pageCnt > 0 && newpage) {
		fwrite( "\n", 1, 1, ofile );
	}
	if ( newpage ) {
		pageCnt++;
		textNoIndent = 1;
		textPutChar( ':' );
		textPutString( stars+strlen(stars)-indent );
		textPutChar( '-' );
		textPutString( title );
		textBreakLine();
		if (context) {
			textPutChar( ':' );
			textPutString( context );
			textPutChar( ' ' );
			textBreakLine();
		}
	}
	textNoIndent = 0;
	xviewBold( title );
	textBreakLine();
	for ( len = strlen(title); len>0; len-- )
		fwrite( "=", 1, 1, ofile );
	fwrite( "\n\n", 1, 2, ofile );
}
void xviewHeader( char * line )
{
	char tmp[1024];
	textSaveLine( tmp );
	textNoIndent = 1;
	textPutChar( ':' );
	textPutString( line );
	textPutChar( ' ' );
	textBreakLine();
	textNoIndent = 0;
	textRestoreLine( tmp );
}
dispatchTable xviewTable = {
	xviewStart,
	normalFinish,
	textNewParagraph,
	textStartLine,
	xviewBold,
	xviewItalic,
	xviewXref,
	(void*)no_op,	/* picture */
	textEndLine,
	textPutChar,
	xviewSection,
	xviewHeader,
	textStartIndent,	/* startDisplay */
	textEndIndent,		/* endDisplay */
	(void*)no_op,
	textStartIndent,	/* listStart */
	textListItem,
	textEndIndent,		/* listEnd */
	(void*)no_op
	};

/******************************************************************************
 *
 *	HTML
 *
 *****************************************************************************/

char * htmlName;
char htmlFileName[1024];

struct {
	char * name;
	int index;
	int section;
	} links[500];
int linkCnt = 0;

void setLink( char * name, int sectionNumber )
{
	links[linkCnt].name = strdup( name );
	links[linkCnt].section = sectionNumber;
	linkCnt++;
}


void getLinks( int sectionNumber, int * prev, int * next )
{
    int cur, inx;

	*prev = -1;
	*next = -1;
	for ( cur = 0; cur < linkCnt; cur++ ) {
		if ( links[cur].section == sectionNumber ) {
			for (inx = cur-1; inx >= 0; inx-- ) {
				if ( strcmp( links[cur].name, links[inx].name ) == 0 ) {
					*prev = links[inx].section;
					break;
				}
			}
			for (inx = cur+1; inx < linkCnt; inx++ ) {
				if ( strcmp( links[cur].name, links[inx].name ) == 0 ) {
					*next = links[inx].section;
					break;
				}
			}
		}
	}
			
}


struct {
	char * name;
	int sectionNumber;
	int subSection;
	} sections[500];
int sectionCnt = 0;
int lastSection = 0;
int curSection = 0;
int subSection = 0;


void defineSection( char * name, int sectionNumber )
{
	if (!name) {
		fprintf( stderr, "%d: NULL context string\n", lineNum );
		return;
	}
	sections[sectionCnt].name = strdup( name );
	sections[sectionCnt].sectionNumber = sectionNumber;
	if (lastSection != sectionNumber) {
		subSection = 0;
	}
	sections[sectionCnt].subSection = subSection++;
	sectionCnt++;
}


int lookupSection( char * name, int *subSection )
{
	int inx;
	if (!name) {
		return -1;
	}
	for (inx=0; inx<sectionCnt; inx++) {
		if (strcmp( name, sections[inx].name ) == 0) {
			*subSection = sections[inx].subSection;
			return sections[inx].sectionNumber;
		}
	}
	fprintf( stderr, "%d: undefined reference to %s\n", lineNum, name );
	return -1;
}


void genHtmlLinks( int sectionNumber, int begin )
{
	int prev, next;
    int comma = 0;

	if (ofile) {
		if (sectionNumber != 0) {
			if (!begin) fprintf( ofile, "\n<p></p><p></p><hr><p>" );
			fprintf( ofile, "<a href=%s.html><b>Return to Contents</b></a>",
					htmlName );
			comma = 1;
		}
		getLinks( sectionNumber, &prev, &next );
		if (prev > 0) {
			if (comma)
				fprintf( ofile, ", " );
			else
				if (!begin) fprintf( ofile, "\n<p></p><p></p><hr><p>" );
			fprintf( ofile, "<a href=%s-%d.html><b>Previous Page</b></a>",
					htmlName, prev );
			comma = 1;
		}
		if (next > 0) {
			if (comma)
				fprintf( ofile, ", " );
			else
				if (!begin) fprintf( ofile, "\n<p></p><p></p><hr><p>" );
			fprintf( ofile, "<a href=%s-%d.html><b>Next Page</b></a>",
					htmlName, next );
			comma = 1;
		}
		if (comma)
			if (begin)
				fprintf( ofile, "</p><hr><p></p>\n" );
			else
				fprintf( ofile, "</p>\n" );
	}
}

int preHtmlSectionNumber = -1;
void preHtmlSection( char section, char * title, char * context, char * picture, char * keywords, int newpage )
{
	if ( !newpage )
		return;
	preHtmlSectionNumber++;
	defineSection( context, preHtmlSectionNumber );
}
void preHtmlHeader( char * line )
{
	if ( line[0] == '*' )
		return;
	defineSection( line, preHtmlSectionNumber );
}
void preHtmlThread( char * thread )
{
	setLink( thread, preHtmlSectionNumber );
}
dispatchTable preHtmlTable = {
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	preHtmlSection,
	preHtmlHeader,
	(void*)no_op,
	(void*)no_op,
	preHtmlThread,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op
	};

dispatchTable htmlTable;

void htmlStart( char * inName, char * outName )
{
	curMode = &preHtmlTable;
	ifile = openFile( inName );
	process( ifile );
	fclose( ifile );
	curMode = &htmlTable;

	ifile = openFile( inName );
	htmlName = outName;
	sprintf( htmlFileName, "%s.html", htmlName );
	ofile = fopen( htmlFileName, "w" );
	if (ofile == NULL) {
		perror( outName );
		exit( 1 );
	}
}
void htmlFinish( void )
{
		genHtmlLinks( curSection, 0 );
}
void htmlNewParagraph( void )
{
    if (wordwrap) {
		if ( listLevel < 0 )
			fprintf( ofile, "<p>" );
		else
			fprintf( ofile, "<br>" );
	} else {
		fprintf( ofile, "\n" );
	}
}
void htmlStartLine( int lastBlank )
{
    if (wordwrap)
	fprintf( ofile, "\n" );
    else
	fprintf( ofile, "\t" );
}
void htmlBold( char * name )
{
	fprintf( ofile, "<b>%s</b>", name );
}
void htmlItalic( char * name )
{
	fprintf( ofile, "<i>%s</i>", name );
}
void htmlXref( char * name, char * ref1, char * ref2 )
{
	int sectionNumber, subSection;
	sectionNumber = lookupSection( ref1, &subSection );
	if (sectionNumber < 0)
		return;
	fprintf( ofile, "<a href=%s", htmlName );
	if (sectionNumber != 0)
		fprintf( ofile, "-%d", sectionNumber );
	fprintf( ofile, ".html" );
	if (subSection != 0)
		fprintf( ofile, "#%d", subSection );
	fprintf( ofile, ">%s</a>", name );
}
void htmlPicture( char * name, int inLine )
{
	fprintf( ofile, "<img src=%s.png>", name );
	if (inLine)
		fprintf( ofile, "\t" );
	else
		fprintf( ofile, "<p></p>\n" );
}
void htmlEndLine( void )
{
	if ( !wordwrap )
		fprintf( ofile, "\n" );
}
void htmlPutChar( char ch )
{
	if ( ch == '<' )
		fprintf( ofile, "&lt;" );
	else if ( ch == '>' )
		fprintf( ofile, "&gt;" );
	else
		fputc( ch, ofile );
}
void htmlSection( char section, char * title, char * context, char * picture, char * keywords, int newpage )
{
	int sectionNumber, subSection;
	if ( newpage ) {
	/*if (line[1] == 'A')*/
		sectionNumber = curSection;
		curSection = lookupSection( context, &subSection );
		if (curSection > 0) {
			genHtmlLinks( sectionNumber, 0 );
			if (ofile)
				fclose( ofile );
			sprintf( htmlFileName, "%s-%d.html", htmlName, curSection );
			ofile = fopen( htmlFileName, "w" );
			if (ofile == NULL) {
				perror( htmlFileName );
				exit(1);
			}
		}
		fprintf( ofile, "<title>%s</title>\n", title );
		genHtmlLinks( curSection, 1 );
	}
	if (picture && picture[0] != '\0')
		fprintf( ofile, "<img src=%s.png>  ", picture );
	fprintf( ofile, "<h%d>%s</h%d>\n",
		line[1]-'A'+1, title, line[1]-'A'+1 );
}
void htmlHeader( char * line )
{
	int sectionNumber, subSection;
	if ( line[0] == '*' )
		return;
	sectionNumber = lookupSection( line, &subSection );
	if (sectionNumber < 0)
		return;
	fprintf( ofile, "<A Name=\"%d\">\n", sectionNumber );
}
void htmlStartDisplay( void )
{
	fprintf( ofile, "<p>\n<pre>" );
}
void htmlEndDisplay( void )
{
	fprintf( ofile, "</pre>\n" );
}
void htmlListStart( void )
{
	fprintf( ofile, "<ul>" );
}
void htmlListItem( void )
{
	fprintf( ofile, "<li>" );
}
void htmlListEnd( void )
{
	fprintf( ofile, "</ul>\n" );
}
dispatchTable htmlTable = {
	htmlStart,
	htmlFinish,
	htmlNewParagraph,
	htmlStartLine,
	htmlBold,
	htmlItalic,
	htmlXref,
	htmlPicture,
	htmlEndLine,
	htmlPutChar,
	htmlSection,
	htmlHeader,
	htmlStartDisplay,
	htmlEndDisplay,
	(void*)no_op,
	htmlListStart,
	htmlListItem,
	htmlListEnd,
	(void*)no_op
	};


/******************************************************************************
 *
 *	DEFINES
 *
 *****************************************************************************/
struct {
	char * name;
	int refCount;
	int lineNum;
	} defs[500];
int defCnt = 0;

void lookupDef( char * name, int def )
{
	int inx;
	if (!name) {
		fprintf( stderr, "%d: NULL context string\n", lineNum );
		return;
	}
	for (inx=0;inx<defCnt;inx++) {
		if (strcmp(defs[inx].name,name)==0) {
			if (def) {
				if (defs[inx].lineNum <= 0)
					defs[inx].lineNum = lineNum;
				else
					fprintf( stderr, "%d: %s redefined (previous %d)\n",
						lineNum, name, defs[inx].lineNum );
			} else {
				defs[inx].refCount++;
			}
			return;
		}
	}
	if (defCnt >= 499) {
		if (defCnt == 499) {
			fprintf( stderr, "%d: too many defines\n", lineNum );
			defCnt++;
		}
		return;
	} else {
		defs[defCnt].name = strdup( name );
		defs[defCnt].lineNum = (def?lineNum:-1);
		defs[defCnt].refCount = 0;
		defCnt++;
	}
}

void defsFinish( void )
{
	int inx;
	for ( inx=0; inx<defCnt; inx++ )
		fprintf( ofile, "%5d: %s [%d]\n",
			defs[inx].lineNum, defs[inx].name, defs[inx].refCount );
	fclose(ofile);
}
void defsSection( char section, char * title, char * context, char * picture, char * keywords, int newpage )
{
	lookupDef( context, 1 );
}
void defsHeader( char * line )
{
	if ( line[0] == '*' )
		return;
	lookupDef( line, 1 );
}
void defsXref( char * name, char * ref1, char * ref2 )
{
	lookupDef( ref1, 0 );
}

dispatchTable defsTable = {
	normalStart,
	defsFinish,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	defsXref,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	defsSection,
	defsHeader,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op,
	(void*)no_op
	};

/******************************************************************************
 *
 *	PARSE
 *
 *****************************************************************************/

char * skipChars( char * cp )
{
	for ( ; *cp; cp++ ) {
		if ( *cp != '$' )
			continue;
		if ( cp[1] == '{' )
			continue;
		break;
	}
	return cp;
}

static int lastlineblank = 0;
void putline( char * line )
{
	int len;
	char * cp, * cq;
	char * name;
	char * mswhelpref;
	char * xvref;
	int sectionNumber;
	int subSection;

	len = strlen(line);
	if (len > 0 && line[len-1] == '\n')
		line[--len] = '\0';
	if (len > 0 && line[len-1] == '\r')
		line[--len] = '\0';
	if (len <= 0) {
		if (lastlineblank)
			return;
		curMode->newParagraph();
		lastlineblank = 1;
		return;
	} else {
		curMode->startLine( lastlineblank );
		lastlineblank = 0;
	}

#ifndef LATER
	if (wordwrap) {
		if (line[len-1] != ' ') {
			line[len++] = ' ';
			if (line[len-2] == '.')
				line[len++] = ' ';
		}
		line[len] = '\0';
	}
#endif

	for ( cp=line; *cp; cp++ ) {
		if (*cp == '$') {
			cp++;
			switch (*cp) {
			case '?':
			case '$':
				curMode->putChar( *cp );
				break;
			case '{':
				curMode->putChar( '$' );
				curMode->putChar( '{' );
				break;
			case 'B':
				name = ++cp;
				cp = skipChars( cp );
				if (*cp=='\0')
					break;
				*cp = '\0';
				curMode->doBold( name );
				break;
			case 'I':
				name = ++cp;
				cp = skipChars( cp );
				if (*cp=='\0')
					break;
				*cp = '\0';
				curMode->doItalic( name );
				break;
			case 'X':
				name = ++cp;
				while (*cp && *cp != '|') cp++;
				if (*cp=='\0')
					break;
				*cp++ = '\0';
				mswhelpref = cp;
				while (*cp && *cp != '|' && *cp != '$') cp++;
				if (*cp=='\0')
					break;
				if (*cp == '|') {
					*cp++ = '\0';
					xvref = cp;
					while (*cp && *cp != '$') cp++;
					if (*cp=='\0')
						break;
					for (cq=xvref; cq<cp; cq++)
						if (*cq==',')
							*cq = '|';
				} else
					xvref = NULL;
				*cp = '\0';
				curMode->doXref( name, mswhelpref, xvref );
				break;
			case 'G':
				name = ++cp;
				while (*cp && *cp != '$') cp++;
				if (*cp=='\0')
					break;
				*cp = '\0';
				curMode->doPicture( name, 1 );
				break;
			default:
				fprintf( stderr, "%d Invalid $ command - %c\n", lineNum, *cp );
				break;
			}
		} else {
			if (*cp != '\014')
				curMode->putChar( *cp );
		}
	}
	curMode->endLine();
}


char * conds[100];
char **condPtr = conds;

void addCond( char * name )
{
	*condPtr++ = name;
}
int lookupCond( char * name )
{
	char ** p;
	int ret = 1;
	if (strlen(name) == 0)
		return 1;
	if (*name == '!') {
		ret = 0;
		name++;
	}
	for (p=conds; p<condPtr; p++) {
		if (strcmp( *p, name )==0)
			return ret;
	}
	return !ret;
}

void process( FILE * f )
{
	char key;
	char * title;
	char * context;
	char * fileName;
	char * keywords;
	char * cp;
	char tmp[1024];
	int indent;
	FILE * newFile;
	int lineNum0;
	int threadCnt;
	int sectionNumber;
	int subSection;
	int valid;
	int sectionNewPage;
	int noSectionNewPage;
	
	lineNum0 = lineNum;
	lineNum = 0;
	while (fgets( line, sizeof line, f ) != NULL) {
		lineNum++;
		line[strlen(line)-1] = '\0';
		if (line[0] == '?' && line[1] == '?') {
			cp = line+2;
			while (isblank(*cp)) cp++;
			if ( strcmp(cp, "else") == 0 )
				valid = !valid;
			else
				valid = lookupCond( cp );
			continue;
		}
		if (!valid) {
			continue;
		}
		if (line[0] == '\014')
			continue;
		if (line[0] != '?') {
			putline( line );
			continue;
		}
		sectionNewPage = 1;
		switch (line[1]) {
		case '#':
			break;
		case '+':
			newFile = openFile( line+2 );
			process( newFile );
			fclose( newFile );
			break;
		case 'a': case 'b': case 'c':
			line[1] += 'A'-'a';
			sectionNewPage = 0;
		case 'A': case 'B': case 'C':
			if ( noSectionNewPage ) {
				sectionNewPage = 0;
				noSectionNewPage = 0;
			}
			context = fileName = keywords = NULL;
			title = cp = line+2;
			while (*cp && *cp != '|') cp++;
			if (*cp) {
				*cp++ = '\0';
				if (*cp!='|')
					context = cp;
			}
			while (*cp && *cp != '|') cp++;
			if (*cp) {
				*cp++ = '\0';
				if (*cp!='|')
					fileName = cp;
			}
			while (*cp && *cp != '|') cp++;
			if (*cp) {
				*cp++ = '\0';
				if (*cp!='|')
					keywords = cp;
			}
			curMode->doSection( line[1], title, context, fileName, keywords, sectionNewPage );
			lastlineblank = 0;
			break;
		case 'H':
			curMode->doHeader( line+2 );
			break;
		case 'W':
			if (line[2] == '+') {
				curMode->doEndDisplay();
				wordwrap = 1;
			} else if (line[2] == '-') {
				curMode->doStartDisplay();
				wordwrap = 0;
			} else {
				fprintf( stderr, "%d: Bad ?W command\n", lineNum);
				exit(1);
			}
			lastlineblank = 0;
			break;
		case 'G':
			curMode->doPicture( line+2, 0 );
			lastlineblank = 0;
			break;
		case 'T':
			curMode->doThread( line+2 );
			break;
		case 'L':
			switch (line[2]) {
			case 'S':
				listLevel++;
				listCount[listLevel] = 0;
				switch (line[3]) {
				case 'o':
					listType[listLevel] = LISTBULLET;
					break;
				case '-':
					listType[listLevel] = LISTDASH;
					break;
				case '1':
					listType[listLevel] = LISTNUMBER;
					break;
				default:
					listType[listLevel] = LISTNONE;
				}
				curMode->doListStart();
				break;
			case 'I':
				if (listLevel<0) {
					fprintf( stderr, "%d: ?LI not in list\n", lineNum );
					break;
				}
				listCount[listLevel]++;
				curMode->doListItem();
				break;
			case 'E':
				listLevel--;
				curMode->doListEnd();
				break;
			}
			lastlineblank = 0;
			break;
		case 'P':
			curMode->page();
			lastlineblank = 0;
			break;
		case 'Q':
			noSectionNewPage = 1;
			break;
		default:
			fprintf( stderr, "%d: Invalid ? command: %c\n", lineNum, line[1] );
		}
	}
	lineNum = lineNum0;
}


/******************************************************************************
 *
 *	MAIN
 *
 *****************************************************************************/

int main ( int argc, char * argv[] )
{
	int inx;

	curMode = NULL;
	argv++; argc--;
	while ( argc > 1 && argv[0][0] == '-' ) {
		if ( strcmp( argv[0], "-xv" ) == 0 ) {
			curMode = &xviewTable;
			addCond( "xv" );
		} else if ( strcmp( argv[0], "-mswhelp" ) == 0 ) {
			curMode = &mswhelpTable;
			addCond( "mswhelp" );
		} else if ( strcmp( argv[0], "-mswword" ) == 0 ) {
			curMode = &mswwordTable;
			addCond( "mswword" );
		} else if ( strcmp( argv[0], "-html" ) == 0 ) {
			curMode = &htmlTable;
			addCond( "html" );
		} else if ( strcmp( argv[0], "-def" ) == 0 ) {
			curMode = &defsTable;
			addCond( "def" );
		} else if ( strcmp( argv[0], "-text" ) == 0 ) {
			curMode = &textTable;
			addCond( "text" );
		} else if ( strncmp( argv[0], "-C", 2 ) == 0 ) {
			argv++; argc--;
			addCond( argv[0] );
		} else if ( strncmp( argv[0], "-v", 2 ) == 0 ) {
			verbose = 1;
		} else if ( strncmp( argv[0], "-d", 2 ) == 0 ) {
			argv++; argc--;
			*dirList++ = argv[0];
		} else if ( strncmp( argv[0], "-width", 2 ) == 0 ) {
			argv++; argc--;
			listWidth = lineWidth = atoi(argv[0]);
			if (lineWidth < 10) {
				fprintf( stderr, "Invalid linewidth %s\n", argv[0] );
				exit(1);
			}
		} else if ( strncmp( argv[0], "-mt", 3 ) == 0 ) {
			argv++; argc--;
			MarginTop = atof( *argv );
		} else if ( strncmp( argv[0], "-mb", 3 ) == 0 ) {
			argv++; argc--;
			MarginBottom = atof( *argv );
		} else if ( strncmp( argv[0], "-mr", 3 ) == 0 ) {
			argv++; argc--;
			MarginRight = atof( *argv );
		} else if ( strncmp( argv[0], "-ml", 3 ) == 0 ) {
			argv++; argc--;
			MarginLeft = atof( *argv );
		} else if ( strncmp( argv[0], "-mg", 3 ) == 0 ) {
			argv++; argc--;
			MarginGutter = atof( *argv );
		} else if ( strncmp( argv[0], "-toc", 4 ) == 0 ) {
			toc++;
		} else {
			fprintf( stderr, "unrecognized option: %s\n", argv[0] );
			exit( 1 );
		}
		argv++;argc--;
	}

	if (curMode == NULL) {
		fprintf( stderr, "Must spec either -mswhelp or -xv\n" );
		exit(1);
	}
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: prochelp [-mswhelp|-xv] <INF> <OUTF>\n" );
		exit( 1 );
	}

	curMode->start( argv[0], argv[1] );
	process( ifile );
	fclose( ifile );
	curMode->finish();
		
	exit(0);
}
