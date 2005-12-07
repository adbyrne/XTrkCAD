#include <stdio.h>
#include <string.h>
#include "readpng.h"

#define PROGNAME "mkshg"


int verbose = 0;
int dmpcolortab = 0;
int dmpimage = 0;

#define MAXRUNLEN (2)

typedef struct {
	unsigned char id0, id1, id2;
	unsigned short x, y, w, h;
	long hash;
	char * name;
	char * context;
	} hotspot_t;

int samecolor( long color1, long color2 )
{
	long c1, c2;
	int i;
	for ( i=0; i<3; i++ ) {
		c1 = (color1&0xFF);
		c2 = (color2&0xFF);
		if ( c1 != c2 ) {
			if ( c1 == 0xFF || c2 == 0xFF ) return FALSE;
			c1 = (c1+1)&0xFE;
			c2 = (c2+1)&0xFE;
			if ( c1 != c2 ) return FALSE;
		}
		color1 >>= 8;
		color2 >>= 8;
	}
	return TRUE;
}

void conv24to8( long * colorTab, char * buff, unsigned long channels, unsigned long width24, unsigned long width8, unsigned long height )
{
	long * lastColor, *cp;
	long color;
	char * ip;
	char *op;
	unsigned long h, w;
	memset( colorTab, 0, 1024 );
	lastColor = colorTab;
	*lastColor++ = 0xC0C0C0;
	*lastColor++ = 0xFFFFFF;
	*lastColor++ = 0x808080;
	*lastColor++ = 0x000000;
	op = buff;
	for (h=0; h<height; h++) {
		ip = buff+(width24*h);
		op = buff+(width8*h);
		for (w=0; w<width24; w+=channels,op++ ) {
			color = ((long)(unsigned char)(ip[0]))<<16;
			color += ((long)(unsigned char)(ip[1]))<<8;
			color += ((long)(unsigned char)(ip[2]));
			ip += channels;
			for ( cp=colorTab; cp<lastColor; cp++ ) {
				if ( samecolor( color, *cp ) ) {
					*op = (char)(cp-colorTab);
					goto nextPixel;
				}
			}
			if (lastColor < &colorTab[256]) {
				*op = (char)(lastColor-colorTab);
				*lastColor++ = color;
			} else {
				*op = 0;
			}
nextPixel:
			if ( dmpimage ) {
				char c;
				if ( *op < 10 )
					printf( "%c", '0'+*op );
				else if ( *op < 10+26 )
					printf( "%c", *op-10+'a' );
				else if ( *op < 10+26+26 )
					printf( "%c", *op-10-26+'A' );
				else
					printf( "\%2.2x", *op );
			}
		}
		*op++ = 0;
		*op++ = 0;
		if ( dmpimage )
			printf( "00\n" );
	}
if ( dmpcolortab ) {
	int i;
	for ( i=0; i<lastColor-colorTab; i++ ) {
		printf( "C[%3d] %6.6lx\n", i, colorTab[i] );
	}
}
} 


void compress_data(
	unsigned char * ip0,
	unsigned long isize,
	unsigned char * op0,
	unsigned long * osize )
{
	unsigned char * ip=ip0, * op=op0;
	int runlen, norunlen, chunk;
	norunlen = 0;
	while ( ip < ip0+isize ) {
		for ( runlen=0; ip+runlen<ip0+isize && ip[0]==ip[runlen]; runlen++ );
		if ( runlen > MAXRUNLEN || runlen == 0 ) {
			while ( norunlen > 0 ) {
				if ( norunlen > 0x7F )
					chunk = 0x7F;
				else
					chunk = norunlen;
				*op++ = 0x80|chunk;
				memcpy( op, ip-norunlen, chunk );
				op += chunk;
				norunlen -= chunk;
			}
			while ( runlen > MAXRUNLEN ) {
				if ( runlen > 0x7F )
					chunk = 0x7F;
				else
					chunk = runlen;
				*op++ = chunk;
				*op++ = *ip;
				ip += chunk;
				runlen -= chunk;
			}
		} else {
			norunlen += runlen;
			ip += runlen;
		}
	}
	*osize = op-op0;
}

	
void writeculong( FILE * shgF, unsigned long value )
{
	unsigned short tmp;
	if ( value > 0x7FFF ) {
		tmp = (unsigned short)((value&0x7FFF)<<1)+1;
		fwrite( &tmp, 2, 1, shgF );
		tmp = (unsigned short)(value>>15);
		fwrite( &tmp, 2, 1, shgF );
	} else {
		tmp = (unsigned short)(value<<1);
		fwrite( &tmp, 2, 1, shgF );
	}
}

void writecushort( FILE * shgF, unsigned short value )
{
	unsigned char tmp;
	if ( value > 0x7F ) {
		tmp = (unsigned short)((value&0x7F)<<1)+1;
		fwrite( &tmp, 1, 1, shgF );
		tmp = (unsigned short)(value>>7);
		fwrite( &tmp, 1, 1, shgF );
	} else {
		tmp = (unsigned short)(value<<1);
		fwrite( &tmp, 1, 1, shgF );
	}
}

void writeShgPic(
	FILE * shgF,
	unsigned long width,
	unsigned long width8,
	unsigned long height,
	long colorTab[256],
	unsigned char * data,
	unsigned short hotspotcnt,
	hotspot_t * hotspots )
{
	short int pictype, packmethod;
	unsigned long xdpi, ydpi, colorsused, colorsimportant, compressedsize, hotspotsize, compressoffset, hotspotoffset;
	unsigned short planes, bitcount;
	unsigned char * compressed_data;
	unsigned int start_off, offset_off, off;
	unsigned short inx;
	unsigned char one = 1;
	unsigned long macrosize = 0;

	pictype = 6;
	packmethod = 1;
	xdpi = ydpi = 96;
	planes = 1;
	bitcount = 8;
	colorsused = 256;
	colorsimportant = 256;
	hotspotsize = (hotspotcnt?7:0);
	hotspotoffset = 0;
	compressed_data = (unsigned char *)malloc( width8*height );
	compress_data( data, width8*height, compressed_data, &compressedsize );
	for ( inx=0; inx<hotspotcnt; inx++ )
		hotspotsize += 15+strlen(hotspots[inx].name)+1+strlen(hotspots[inx].context)+1;

	start_off = ftell( shgF );
	fwrite( &pictype, 1, 1, shgF );
	fwrite( &packmethod, 1, 1, shgF );
	writeculong( shgF, xdpi );
	writeculong( shgF, ydpi );
	writecushort( shgF, planes );
	writecushort( shgF, bitcount );
	writeculong( shgF, width );
	writeculong( shgF, height );
	writeculong( shgF, colorsused );
	writeculong( shgF, colorsimportant );
	writeculong( shgF, compressedsize );
	writeculong( shgF, hotspotsize );
	offset_off = ftell( shgF );
	compressoffset = offset_off-start_off+8+colorsused*4;
	fwrite( &compressoffset, 4, 1, shgF );
	fwrite( &hotspotoffset, 4, 1, shgF );
	if ( verbose )
	    printf( "TY=%d, PK=%d, XD=%ld, YD=%ld, PL=%d, BC=%d, W=%ld, H=%ld, CU=%ld, CI=%ld, CS=%ld, HS=%ld, CO=%ld, HO=%ld\n",
		pictype, packmethod, xdpi, ydpi, planes, bitcount, width, height, colorsused, colorsimportant, compressedsize, hotspotsize, compressoffset, hotspotoffset );
	fwrite( colorTab, colorsused, 4, shgF );
	fwrite( compressed_data, compressedsize, 1, shgF );
	if ( hotspotcnt>0 ) {
		hotspotoffset = ftell( shgF ) - start_off;
		fwrite( &one, 1, 1, shgF );
		fwrite( &hotspotcnt, 2, 1, shgF );
		fwrite( &macrosize, 4, 1, shgF );
		for ( inx=0; inx<hotspotcnt; inx++ ) {
			fwrite( &hotspots[inx].id0, 1, 1, shgF );
			fwrite( &hotspots[inx].id1, 1, 1, shgF );
			fwrite( &hotspots[inx].id2, 1, 1, shgF );
			fwrite( &hotspots[inx].x, 2, 1, shgF );
			fwrite( &hotspots[inx].y, 2, 1, shgF );
			fwrite( &hotspots[inx].w, 2, 1, shgF );
			fwrite( &hotspots[inx].h, 2, 1, shgF );
			fwrite( &hotspots[inx].hash, 4, 1, shgF );
		}
		for ( inx=0; inx<hotspotcnt; inx++ ) {
			fwrite( hotspots[inx].name, strlen(hotspots[inx].name)+1, 1, shgF );
			fwrite( hotspots[inx].context, strlen(hotspots[inx].context)+1, 1, shgF );
		}
		fseek( shgF, offset_off+4, SEEK_SET );
		fwrite( &hotspotoffset, 4, 1, shgF );
	}

	free( compressed_data );
}


signed char hashTable[256]=
{
    '\x00', '\xD1', '\xD2', '\xD3', '\xD4', '\xD5', '\xD6', '\xD7',
    '\xD8', '\xD9', '\xDA', '\xDB', '\xDC', '\xDD', '\xDE', '\xDF',
    '\xE0', '\xE1', '\xE2', '\xE3', '\xE4', '\xE5', '\xE6', '\xE7',
    '\xE8', '\xE9', '\xEA', '\xEB', '\xEC', '\xED', '\xEE', '\xEF',
    '\xF0', '\x0B', '\xF2', '\xF3', '\xF4', '\xF5', '\xF6', '\xF7',
    '\xF8', '\xF9', '\xFA', '\xFB', '\xFC', '\xFD', '\x0C', '\xFF',
    '\x0A', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
    '\x08', '\x09', '\x0A', '\x0B', '\x0C', '\x0D', '\x0E', '\x0F',
    '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
    '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', '\x1E', '\x1F',
    '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27',
    '\x28', '\x29', '\x2A', '\x0B', '\x0C', '\x0D', '\x0E', '\x0D',
    '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
    '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', '\x1E', '\x1F',
    '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27',
    '\x28', '\x29', '\x2A', '\x2B', '\x2C', '\x2D', '\x2E', '\x2F',
    '\x50', '\x51', '\x52', '\x53', '\x54', '\x55', '\x56', '\x57',
    '\x58', '\x59', '\x5A', '\x5B', '\x5C', '\x5D', '\x5E', '\x5F',
    '\x60', '\x61', '\x62', '\x63', '\x64', '\x65', '\x66', '\x67',
    '\x68', '\x69', '\x6A', '\x6B', '\x6C', '\x6D', '\x6E', '\x6F',
    '\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77',
    '\x78', '\x79', '\x7A', '\x7B', '\x7C', '\x7D', '\x7E', '\x7F',
    '\x80', '\x81', '\x82', '\x83', '\x0B', '\x85', '\x86', '\x87',
    '\x88', '\x89', '\x8A', '\x8B', '\x8C', '\x8D', '\x8E', '\x8F',
    '\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97',
    '\x98', '\x99', '\x9A', '\x9B', '\x9C', '\x9D', '\x9E', '\x9F',
    '\xA0', '\xA1', '\xA2', '\xA3', '\xA4', '\xA5', '\xA6', '\xA7',
    '\xA8', '\xA9', '\xAA', '\xAB', '\xAC', '\xAD', '\xAE', '\xAF',
    '\xB0', '\xB1', '\xB2', '\xB3', '\xB4', '\xB5', '\xB6', '\xB7',
    '\xB8', '\xB9', '\xBA', '\xBB', '\xBC', '\xBD', '\xBE', '\xBF',
    '\xC0', '\xC1', '\xC2', '\xC3', '\xC4', '\xC5', '\xC6', '\xC7',
    '\xC8', '\xC9', '\xCA', '\xCB', '\xCC', '\xCD', '\xCE', '\xCF'
};

void readHotspots(
	FILE * inF,
	unsigned short * hotspotcnt,
	hotspot_t * * hotspotptr )
{
	hotspot_t hs, *hs_p;
	int x, y;
	unsigned short hs_c;
	char type[80];
	char name[80];
	char line[256], *context;
	int off;
	int rc;

	hs_c = 0;
	hs_p = NULL;
	while ( fgets( line, sizeof line, inF ) != NULL ) {
		rc=sscanf( line, "%s %hd %hd %hd %hd %n", type, &x, &y, &hs.w, &hs.h, &off );
		if ( hs_c > 0 ) {
			if ( x < 0 )
			    x = ((int)hs_p[hs_c-1].x) - x;
			if ( y < 0 )
			    y = ((int)hs_p[hs_c-1].y) - y;
		}
		hs.x = (unsigned short)x;
		hs.y = (unsigned short)y;
		if ( rc != 5 )
			fprintf( stderr, "Invalid hotspot syntax: %s", line );
		if ( strcasecmp( type, "jump" ) == 0 ) {
			hs.id0 = 0xe7;
			hs.id1 = 0x04;
			hs.id2 = 0x00;
		} else if ( strcasecmp( type, "popup" ) == 0 ) {
			hs.id0 = 0xe6;
			hs.id1 = 0x04;
			hs.id2 = 0x00;
		} else if ( strcasecmp( type, "ignore" ) == 0 ) {
			continue;
		} else {
			fprintf( stderr, "Invalid hotspot type: %s", line );
			continue;
		}
		sprintf( name, "Hotspot %d", hs_c+1 );
		hs.name = strdup( name );
		context = line+off;
		off = strlen( context );
		if ( context[off-1] == '\n' ) context[off-1] = 0;
		hs.context = strdup( context );
		for (hs.hash=0; *context; context++ )
			hs.hash = (hs.hash*43)+hashTable[(unsigned char)*context];
		hs_c++;
		hs_p = (hotspot_t*)realloc( hs_p, hs_c * sizeof hs );
		hs_p[hs_c-1] = hs;
	}
	*hotspotcnt = hs_c;
	*hotspotptr = hs_p;
}


void PngToShg(
	char * pngFile,
	char * shgFile )
{
	FILE * pngF, * shgF;
	int rc;
	unsigned long image_width, image_height, image_rowbytes, width8, h;
	int image_channels;
	unsigned char * image_data;
	double display_exponent = 1.0;

	long size, fileSize, maxRecSize;
	long colorTab[256];

	short int magic;
	short int piccnt;
	long int picoff[1];

	unsigned short hotspotcnt;
	hotspot_t * hotspotptr;

	pngF = fopen( pngFile, "r" );
	if ( pngF == NULL ) {
		perror( pngFile );
		return;
	}
	shgF = fopen( shgFile, "w" );
	if ( shgF == NULL ) {
		perror( shgFile );
		return;
	}
        if ((rc = readpng_init(pngF, &image_width, &image_height)) != 0) {
            switch (rc) {
                case 1:
                    fprintf(stderr, PROGNAME
                      ":  [%s] is not a PNG file: incorrect signature\n",
                      pngFile);
                    break;
                case 2:
                    fprintf(stderr, PROGNAME
                      ":  [%s] has bad IHDR (libpng longjmp)\n",
                      pngFile);
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

	conv24to8( colorTab, image_data, image_channels, image_width*image_channels, width8, image_height );

	magic = 0x706c;
	piccnt = 1;
	picoff[0] = 8;
	fwrite( &magic, 2, 1, shgF );
	fwrite( &piccnt, 2, 1, shgF );
	fwrite( picoff, 4, piccnt, shgF );
	readHotspots( stdin, &hotspotcnt, &hotspotptr );
	writeShgPic( shgF, image_width, width8, image_height, colorTab, image_data, hotspotcnt, hotspotptr );
	if ( hotspotptr )
		free( hotspotptr );

	readpng_cleanup(FALSE);
	fclose( pngF );
	fclose( shgF );
	free( image_data );

}

int main( argc, argv )
int argc; char * argv[];
{
    while ( argc > 1 && argv[1][0] == '-' ) {
	switch ( argv[1][1] ) {
	case 'v':
		verbose++;
		break;
	case 'c':
		dmpcolortab++;
		break;
	case 'i':
		dmpimage++;
		break;
	default:
		fprintf( stderr, "Unknown option: %s\n", argv[1] );
		exit(1);
	}
	argv++;
	argc--;
    }
    if ( argc != 3 ) {
	fprintf( stderr, "Usage: mkshg [-v] [-c] infile.png outfile.shg\n" );
	exit(1);
    }

    PngToShg( argv[1], argv[2] );
}
